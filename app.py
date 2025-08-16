from flask import Flask, request, jsonify
import subprocess
import threading
import time
import os
from uuid import uuid4

class FlaskLED(Flask):
  def run(self, host=None, port=None, debug=None, load_dotenv=True, **options):
    if not self.debug or os.getenv('WERKZEUG_RUN_MAIN') == 'true':
      with self.app_context():
          for _ in range(3):
            set_led(1)
            time.sleep(0.1)
            set_led(0)
            time.sleep(0.1)
    super(FlaskLED, self).run(host=host, port=port, debug=debug, load_dotenv=load_dotenv, **options)

app = FlaskLED(__name__)

def get_led(name: str= None):
    args= ['led_tcp_control']
    if name:
        args.extend(['--name', name])
    return subprocess.run(args, capture_output=True, text=True).stdout.strip()
   
def set_led(val: int, name: str= None):
    args = ['led_tcp_control']
    if name:
        args.extend(['--name', name])
    args.append(str(val))
    return subprocess.run(args, capture_output=True, text=True).stdout.strip()

def get_led_list():
    result = subprocess.run(['led_tcp_control', '--list'], capture_output=True, text=True)
    # Assuming output is one LED name per line
    return [line.strip() for line in result.stdout.splitlines() if line.strip()]

def led_blink(led_name:str, cnt: int, delay: int, val: str, stop_event): 
    delay_on=int(val)/100*delay/1000
    delay_off=(100-int(val))/100*delay/1000
    for _ in range(cnt):
        if stop_event.is_set():
            break
        set_led(1, led_name)
        time.sleep(delay_on)
        set_led(0, led_name)
        time.sleep(delay_off)

def led_pattern(thread_id, led_name, _type: str, cnt: int, delay: int, val: str, stop_event):
    original=get_led(led_name)
    if _type not in ["blink"]:
        return
    led_blink(led_name, cnt, delay, val, stop_event)
    set_led(original, led_name)

@app.route('/')
def hello_world():
    return 'Hi'

AVAILABLE_LEDS = get_led_list()
@app.route('/leds', methods=['GET'])
def list_leds():
    AVAILABLE_LEDS = get_led_list()
    return jsonify({"leds": AVAILABLE_LEDS})


def get_led_name():
    led_name = request.args.get("name")
    if led_name is None:
        led_name = AVAILABLE_LEDS[0] if AVAILABLE_LEDS else None
    if not led_name or led_name not in AVAILABLE_LEDS:
        return jsonify({"error": "Invalid or missing LED name"}), 400
    return led_name

@app.route('/led', methods=['POST', 'GET'])
def led_controller():
    led_name = get_led_name()
    if request.method == 'GET':
        return jsonify({"state": get_led(led_name)})
    if request.method == 'POST':
        data = request.get_json()
        if 'status' in data:
            set_led(1 if data['status']=="on" else 0)
            return jsonify({"message": "LED state updated", "state": get_led(led_name)})
        else:
            return jsonify({"error": "Invalid data"}), 400

threads = []

@app.route('/led/pattern', methods=['POST'])
def led_controller_pattern():
    led_name = get_led_name()
    if request.method == 'POST':
        data = request.get_json()
        fileds=["type", "cnt", 'delay', 'val']
        for f in fileds:
            if f not in data:
                return f"missing {f}"
        if data["type"] not in ["blink"]:
            return "The type is not implemented!"
        id = uuid4().hex
        stop_event = threading.Event()
        t = threading.Thread(target=led_pattern, args=(id, led_name, data["type"], data["cnt"], data["delay"], data["val"],stop_event))
        threads.append((led_name,t, stop_event))
        t.start()       
        return jsonify({"message": "set"})

@app.route('/led/pattern', methods=['DELETE'])
def led_controller_pattern_evict():
        led_name = get_led_name()
        for name, t, e in threads:
            if name != led_name:
                continue
            e.set()
            t.join()
        return jsonify({"message": "removed all"})


# main driver function
if __name__ == '__main__':

    # run() method of Flask class runs the application 
    # on the local development server.
    app.run(host="0.0.0.0", port=4040)
