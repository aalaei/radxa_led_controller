
from flask import Flask, request, jsonify
import subprocess
import threading
import time
from uuid import uuid4

class FlaskLED(Flask):
  def run(self, host=None, port=None, debug=None, load_dotenv=True, **options):
    if not self.debug or os.getenv('WERKZEUG_RUN_MAIN') == 'true':
      with self.app_context():
          set_led(1)
          time.sleep(0.1)
          set_led(0)
          time.sleep(0.1)
          set_led(1)
          time.sleep(0.1)
          set_led(0)
    super(FlaskLED, self).run(host=host, port=port, debug=debug, load_dotenv=load_dotenv, **options)

app = FlaskLED(__name__)

def get_led():
    return subprocess.run(['led_tcp_control'], capture_output=True, text=True).stdout

def set_led(val:int):
    return subprocess.run(['led_tcp_control', str(val)], capture_output=True, text=True).stdout

def led_blink(cnt: int, delay: int, val: str, stop_event): 
    delay_on=int(val)/100*delay/1000
    delay_off=(100-int(val))/100*delay/1000
    for _ in range(cnt):
        if stop_event.is_set():
            break
        set_led(1)
        time.sleep(delay_on)
        set_led(0)
        time.sleep(delay_off)

def led_pattern(thread_id, _type: str, cnt: int, delay: int, val: str, stop_event):
    original=get_led()
    if _type not in ["blink"]:
        return
    led_blink(cnt, delay, val, stop_event)
    set_led(original)

@app.route('/')
def hello_world():
    return 'Hi'

@app.route('/led', methods=['POST', 'GET'])
def led_controller():
    if request.method == 'GET':
        return jsonify({"state": get_led()})
    if request.method == 'POST':
        data = request.get_json()
        if 'status' in data:
            set_led(1 if data['status']=="on" else 0)
            return jsonify({"message": "LED state updated", "state": get_led()})
        else:
            return jsonify({"error": "Invalid data"}), 400

threads = []

@app.route('/led/pattern', methods=['POST'])
def led_controller_pattern():
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
        t = threading.Thread(target=led_pattern, args=(id, data["type"], data["cnt"], data["delay"], data["val"],stop_event))
        threads.append((t, stop_event))
        t.start()       
        return jsonify({"message": "set"})

@app.route('/led/pattern', methods=['DELETE'])
def led_controller_pattern_evict():
        for t, e in threads:
            e.set()
            t.join()
        return jsonify({"message": "removed all"})


# main driver function
if __name__ == '__main__':

    # run() method of Flask class runs the application 
    # on the local development server.
    app.run(host="0.0.0.0", port=4040)
