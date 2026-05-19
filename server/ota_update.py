import paho.mqtt.client as mqtt
import argparse

BROKER_HOST = "laptop.local"
BROKER_PORT = 1883
HTTP_PORT = 8080

def publish_ota(node_id, firmware):
    url = f"http://laptop.local:{HTTP_PORT}/{firmware}"
    topic = f"airquality/node{node_id}/ota"
    
    client = mqtt.Client(
        client_id="ota-publisher",
        callback_api_version=mqtt.CallbackAPIVersion.VERSION2
    )
    client.connect(BROKER_HOST, BROKER_PORT)
    client.publish(topic, url, qos=1)
    client.disconnect()
    print(f"OTA triggered for node {node_id}")
    print(f"URL: {url}")
    print(f"Topic: {topic}")

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--node", type=int, required=True)
    parser.add_argument("--firmware", default="air-quality-monitoring.bin")
    args = parser.parse_args()
    publish_ota(args.node, args.firmware)
