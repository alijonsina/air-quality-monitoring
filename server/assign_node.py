import paho.mqtt.client as mqtt
import argparse
import time

BROKER_HOST = "localhost"
BROKER_PORT = 1883

def assign_node(node_id):
    client = mqtt.Client(
        client_id="node-assigner",
        callback_api_version=mqtt.CallbackAPIVersion.VERSION2
    )
    client.connect(BROKER_HOST, BROKER_PORT)
    client.loop_start()
    time.sleep(1)
    
    payload = f'{{"assign_id":{node_id}}}'
    client.publish("airquality/unassigned/config", payload, qos=1)
    print(f"Assigned node ID {node_id}")
    
    time.sleep(1)
    client.loop_stop()
    client.disconnect()

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--id", type=int, required=True)
    args = parser.parse_args()
    assign_node(args.id)
