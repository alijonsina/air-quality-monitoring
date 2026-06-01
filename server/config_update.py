import paho.mqtt.client as mqtt
import argparse
import time
import json

BROKER_HOST = "localhost"
BROKER_PORT = 1883

def send_config(node_id, key, value):
    topic = f"airquality/node{node_id}/config"
    payload = json.dumps({key: value})

    client = mqtt.Client(
        client_id="config-publisher",
        callback_api_version=mqtt.CallbackAPIVersion.VERSION2
    )
    client.connect(BROKER_HOST, BROKER_PORT)
    client.loop_start()
    time.sleep(0.5)
    client.publish(topic, payload, qos=1)
    time.sleep(0.5)
    client.loop_stop()
    client.disconnect()
    print(f"Config sent to node {node_id}: {payload}")

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--node", type=int, required=True)
    parser.add_argument("--rzero", type=float, default=None)
    parser.add_argument("--interval", type=int, default=None)
    args = parser.parse_args()

    if args.rzero is not None:
        send_config(args.node, "rzero", args.rzero)
    elif args.interval is not None:
        send_config(args.node, "read_interval", args.interval)
    else:
        print("Specify --rzero or --interval")
