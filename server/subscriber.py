import paho.mqtt.client as mqtt
import sqlite3
import json
import logging
import os
from datetime import datetime

BROKER_HOST = "localhost"
BROKER_PORT = 1883
TOPIC       = "airquality/#"
DB_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), "database", "airquality.db")

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] %(message)s"
)
log = logging.getLogger(__name__)

def init_db():
    os.makedirs(os.path.dirname(DB_PATH), exist_ok=True)
    with sqlite3.connect(DB_PATH) as conn:
        cursor = conn.cursor()
        cursor.execute("""
            CREATE TABLE IF NOT EXISTS airquality_table (
                id          INTEGER PRIMARY KEY AUTOINCREMENT,
                node_id     INTEGER NOT NULL,
                timestamp   INTEGER NOT NULL,
                ppm         REAL NOT NULL,
                temperature REAL NOT NULL,
                humidity    REAL NOT NULL,
                received_at DATETIME DEFAULT CURRENT_TIMESTAMP
            )
        """)
        conn.commit()
        log.info("Database initialised")

def connect_mqtt() -> mqtt_client:
    def on_connect(client, userdata, flags, reason_code, properties):
        if reason_code == 0:
            log.info("Connected to MQTT broker")
            client.subscribe(TOPIC)
            log.info(f"Subscribed to {TOPIC}")
        else:
            log.error(f"Failed to connect, reason code: {reason_code}")

    def on_disconnect(client, userdata, flags, reason_code, properties):
        log.warning(f"Disconnected from broker, reason code: {reason_code}")

    client = mqtt.Client(
        client_id="airquality-subscriber",
        callback_api_version=mqtt.CallbackAPIVersion.VERSION2,
    )
    client.on_connect = on_connect
    client.on_disconnect = on_disconnect
    client.connect(BROKER_HOST, BROKER_PORT)
    return client


def on_message(client, userdata, message):
    try:
        data = json.loads(message.payload.decode('utf-8'))
    except json.JSONDecodeError:
        log.error(f"Invalid JSON received: {message.payload}")
        return

    required_fields = ["node", "ppm", "temperature", "humidity", "timestamp"]
    if not all(field in data for field in required_fields):
        log.error(f"Missing fields in payload: {data}")
        return

    try:
        with sqlite3.connect(DB_PATH) as conn:
            conn.execute("""
                INSERT INTO airquality_table 
                (node_id, ppm, temperature, humidity, timestamp)
                VALUES (?, ?, ?, ?, ?)""", 
                (data["node"], data["ppm"], data["temperature"], data["humidity"], data["timestamp"]))
            conn.commit()
            log.info(f"Node {data['node']} | PPM: {data['ppm']} | "
                     f"Temp: {data['temperature']}°C | "
                     f"Humidity: {data['humidity']}%")
    except sqlite3.Error as e:
        log.error(f"Database error: {e}")

def main():
    init_db()
    
    client = connect_mqtt()
    client.on_message = on_message
    
    try:
        client.loop_forever()
    except KeyboardInterrupt:
        log.info("Subscriber stopped")
        client.disconnect()

if __name__ == "__main__":
    main()
