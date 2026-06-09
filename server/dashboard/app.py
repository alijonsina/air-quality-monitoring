from flask import Flask, render_template, jsonify, request, Response
import sqlite3
import json
import paho.mqtt.client as mqtt
import threading
import queue
import time
import os
import subprocess
import sys
from datetime import datetime

app = Flask(__name__)

# ─────────────────────────────────────────
# Configuration
# ─────────────────────────────────────────

DASHBOARD_DIR = os.path.dirname(os.path.abspath(__file__))
DB_PATH       = os.path.normpath(os.path.join(DASHBOARD_DIR, "..", "database", "airquality.db"))
BROKER_HOST   = "localhost"
BROKER_PORT   = 1883

listeners       = []
mqtt_client_ref = None

# ─────────────────────────────────────────
# Database
# ─────────────────────────────────────────

def get_db():
    conn = sqlite3.connect(DB_PATH)
    conn.row_factory = sqlite3.Row
    return conn

# ─────────────────────────────────────────
# Subscriber process
# ─────────────────────────────────────────

def start_subscriber():
    subscriber_path = os.path.normpath(
        os.path.join(DASHBOARD_DIR, "..", "subscriber.py")
    )
    while True:
        try:
            print(f"Starting subscriber: {subscriber_path}")
            subprocess.run([sys.executable, subscriber_path])
        except Exception as e:
            print(f"Subscriber crashed: {e}, restarting in 5s")
            time.sleep(5)

# ─────────────────────────────────────────
# MQTT
# ─────────────────────────────────────────

def on_message(client, userdata, message):
    try:
        data = json.loads(message.payload.decode("utf-8"))
        if "ppm" in data:
            for listener in listeners[:]:
                listener(data)
    except json.JSONDecodeError:
        pass

def on_connect(client, userdata, flags, reason_code, properties):
    if reason_code == 0:
        client.subscribe("airquality/#")

def start_mqtt():
    global mqtt_client_ref
    client = mqtt.Client(
        client_id="dashboard",
        callback_api_version=mqtt.CallbackAPIVersion.VERSION2
    )
    client.on_message = on_message
    client.on_connect = on_connect
    mqtt_client_ref   = client

    while True:
        try:
            client.connect(BROKER_HOST, BROKER_PORT)
            client.loop_forever()
        except Exception as e:
            print(f"MQTT connection failed: {e}, retrying in 5s")
            time.sleep(5)

# ─────────────────────────────────────────
# SSE
# ─────────────────────────────────────────

@app.route("/stream")
def stream():
    def event_stream():
        client_queue = queue.Queue()

        def forward(data):
            client_queue.put(data)

        listeners.append(forward)
        try:
            while True:
                try:
                    data = client_queue.get(timeout=30)
                    yield f"data: {json.dumps(data)}\n\n"
                except queue.Empty:
                    yield "data: {\"keepalive\": true}\n\n"
        except GeneratorExit:
            listeners.remove(forward)

    return Response(
        event_stream(),
        mimetype="text/event-stream",
        headers={
            "Cache-Control":     "no-cache",
            "X-Accel-Buffering": "no"
        }
    )

# ─────────────────────────────────────────
# API
# ─────────────────────────────────────────

@app.route("/api/nodes")
def api_nodes():
    try:
        with get_db() as conn:
            rows = conn.execute("""
                SELECT node_id, ppm, temperature, humidity,
                       timestamp, received_at,
                       MAX(received_at) as latest
                FROM airquality_table
                GROUP BY node_id
                ORDER BY node_id
            """).fetchall()

            now = datetime.now()
            nodes = []
            for row in rows:
                last_seen = datetime.strptime(
                    row["received_at"], "%Y-%m-%d %H:%M:%S"
                )
                seconds_ago = (now - last_seen).total_seconds()
                online = seconds_ago < 120

                hours_ago = int(seconds_ago // 3600)
                mins_ago  = int((seconds_ago % 3600) // 60)
                if hours_ago > 0:
                    time_ago = f"{hours_ago}h {mins_ago}m ago"
                else:
                    time_ago = f"{mins_ago}m ago"

                nodes.append({
                    "node_id":     row["node_id"],
                    "ppm":         row["ppm"],
                    "temperature": row["temperature"],
                    "humidity":    row["humidity"],
                    "timestamp":   row["timestamp"],
                    "received_at": row["received_at"],
                    "online":      online,
                    "time_ago":    time_ago
                })
            return jsonify(nodes)
    except Exception as e:
        return jsonify({"error": str(e)}), 500

@app.route("/api/node/<int:node_id>/history")
def api_node_history(node_id):
    try:
        hours = request.args.get("hours", 24, type=float)
        with get_db() as conn:
            rows = conn.execute("""
                SELECT ppm, temperature, humidity, timestamp, received_at
                FROM airquality_table
                WHERE node_id = ?
                AND received_at >= datetime('now', ? || ' hours')
                ORDER BY received_at ASC
            """, (node_id, f"-{hours}")).fetchall()

            return jsonify([{
                "ppm":         row["ppm"],
                "temperature": row["temperature"],
                "humidity":    row["humidity"],
                "timestamp":   row["timestamp"],
                "received_at": row["received_at"]
            } for row in rows])
    except Exception as e:
        return jsonify({"error": str(e)}), 500

@app.route("/api/node/<int:node_id>/history/range")
def api_node_history_range(node_id):
    try:
        start = request.args.get("start")
        end   = request.args.get("end")
        if not start or not end:
            return jsonify({"error": "start and end required"}), 400

        with get_db() as conn:
            rows = conn.execute("""
                SELECT ppm, temperature, humidity, timestamp, received_at
                FROM airquality_table
                WHERE node_id = ?
                AND received_at >= ? AND received_at <= ?
                ORDER BY received_at ASC
            """, (node_id, start, end)).fetchall()

            return jsonify([{
                "ppm":         row["ppm"],
                "temperature": row["temperature"],
                "humidity":    row["humidity"],
                "timestamp":   row["timestamp"],
                "received_at": row["received_at"]
            } for row in rows])
    except Exception as e:
        return jsonify({"error": str(e)}), 500

@app.route("/api/averages")
def api_averages():
    try:
        hours = request.args.get("hours", 24, type=float)
        with get_db() as conn:
            rows = conn.execute("""
                SELECT
                    strftime('%Y-%m-%d %H:%M:00', received_at) as time_bucket,
                    AVG(ppm)         as avg_ppm,
                    AVG(temperature) as avg_temperature,
                    AVG(humidity)    as avg_humidity
                FROM airquality_table
                WHERE received_at >= datetime('now', ? || ' hours')
                GROUP BY time_bucket
                ORDER BY time_bucket ASC
            """, (f"-{hours}",)).fetchall()

            return jsonify([{
                "time":            row["time_bucket"],
                "avg_ppm":         round(row["avg_ppm"], 2),
                "avg_temperature": round(row["avg_temperature"], 2),
                "avg_humidity":    round(row["avg_humidity"], 2)
            } for row in rows])
    except Exception as e:
        return jsonify({"error": str(e)}), 500

@app.route("/api/averages/range")
def api_averages_range():
    try:
        start = request.args.get("start")
        end   = request.args.get("end")
        if not start or not end:
            return jsonify({"error": "start and end required"}), 400

        with get_db() as conn:
            rows = conn.execute("""
                SELECT
                    strftime('%Y-%m-%d %H:%M:00', received_at) as time_bucket,
                    AVG(ppm)         as avg_ppm,
                    AVG(temperature) as avg_temperature,
                    AVG(humidity)    as avg_humidity
                FROM airquality_table
                WHERE received_at >= ? AND received_at <= ?
                GROUP BY time_bucket
                ORDER BY time_bucket ASC
            """, (start, end)).fetchall()

            return jsonify([{
                "time":            row["time_bucket"],
                "avg_ppm":         round(row["avg_ppm"], 2),
                "avg_temperature": round(row["avg_temperature"], 2),
                "avg_humidity":    round(row["avg_humidity"], 2)
            } for row in rows])
    except Exception as e:
        return jsonify({"error": str(e)}), 500

@app.route("/api/node/<int:node_id>/config", methods=["POST"])
def api_node_config(node_id):
    try:
        data = request.get_json()
        if not data:
            return jsonify({"error": "No data"}), 400

        if mqtt_client_ref and mqtt_client_ref.is_connected():
            mqtt_client_ref.publish(
                f"airquality/node{node_id}/config",
                json.dumps(data),
                qos=1
            )
            return jsonify({"status": "sent"})
        else:
            return jsonify({"error": "MQTT not connected"}), 503
    except Exception as e:
        return jsonify({"error": str(e)}), 500

# ─────────────────────────────────────────
# Pages
# ─────────────────────────────────────────

@app.route("/")
def index():
    return render_template("index.html")

@app.route("/node/<int:node_id>")
def node_detail(node_id):
    return render_template("node.html", node_id=node_id)

# ─────────────────────────────────────────
# Entry point
# ─────────────────────────────────────────

if __name__ == "__main__":
    mqtt_thread = threading.Thread(target=start_mqtt, daemon=True)
    mqtt_thread.start()

    subscriber_thread = threading.Thread(target=start_subscriber, daemon=True)
    subscriber_thread.start()

    time.sleep(1)

    print(f"DB_PATH: {DB_PATH}")
    print(f"DB exists: {os.path.exists(DB_PATH)}")
    print("Dashboard running at http://localhost:5000")

    app.run(host="0.0.0.0", port=5000, debug=False)
