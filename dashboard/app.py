"""Local USB-serial dashboard. Run python app.py, then open 127.0.0.1:5000."""
import argparse
import base64
import binascii
from collections import deque
from datetime import datetime, timezone
import io
import json
import math
from pathlib import Path
import secrets
import threading
import time
import zlib

from flask import Flask, jsonify, render_template, request, send_file
import serial
from serial.tools import list_ports

MAX_IMAGE = 24576

class Station:
    def __init__(self):
        self.lock = threading.RLock()
        self.connection = None
        self.thread = None
        self.stop_event = threading.Event()
        self.logs = deque(maxlen=150)
        self.samples = deque(maxlen=300)
        self.image = None
        self.image_version = 0
        self.received = self.total = 0
        self.rssi = None
        self.busy = False
        self.command_at = 0
        self.last_event = 'Connect the ground station USB port.'
        self.image_bytes = 0
        self.port = ''

    def log(self, message):
        with self.lock:
            self.last_event = str(message)[:220]
            self.logs.appendleft({'time': datetime.now().astimezone().isoformat(timespec='seconds'), 'message': self.last_event})

    def connect(self, port):
        self.disconnect()
        connection = serial.Serial(port=port, baudrate=115200, timeout=0.5, write_timeout=2)
        # NodeMCU may reset when the serial port opens. Allow its boot output to finish.
        time.sleep(2)
        connection.reset_input_buffer()
        with self.lock:
            self.connection = connection
            self.port = port
            self.stop_event = threading.Event()
            self.thread = threading.Thread(target=self.read_loop, args=(connection, self.stop_event), daemon=True)
            self.thread.start()
        self.log('USB serial connected: ' + port)
        connection.write(b'PING\n')

    def disconnect(self):
        with self.lock:
            old, thread, stop = self.connection, self.thread, self.stop_event
            self.connection = None
            self.thread = None
            self.busy = False
            self.port = ''
            stop.set()
        if old:
            try:
                old.close()
            except serial.SerialException:
                pass
        if thread and thread is not threading.current_thread():
            thread.join(timeout=2)

    def command(self, command):
        with self.lock:
            if not self.connection or not self.connection.is_open:
                raise ValueError('Connect the USB serial port first.')
            if self.busy:
                raise ValueError('A transfer is already in progress.')
            self.connection.write((command + '\n').encode('ascii'))
            self.busy = True
            self.command_at = time.monotonic()
            self.received = self.total = 0
        self.log('Requested sensor values.' if command == 'SENSOR' else 'Requested camera image.')

    def ingest(self, event):
        if not isinstance(event, dict):
            return
        kind = event.get('type')
        with self.lock:
            if kind == 'sensor':
                values = event.get('values', {})
                if not isinstance(values, dict):
                    raise ValueError('Invalid sensor payload')
                temperature = float(values['temperature'])
                humidity = float(values['humidity'])
                if not math.isfinite(temperature) or not math.isfinite(humidity) or not 0 <= humidity <= 100:
                    raise ValueError('Invalid sensor reading')
                self.samples.append({'time': datetime.now().astimezone().isoformat(timespec='seconds'), 'temperature': temperature, 'humidity': humidity})
                self.rssi = event.get('rssi')
                self.busy = False
                self.log('Sensor values received and acknowledged.')
            elif kind == 'progress':
                received, total = int(event['received']), int(event['total'])
                if not 0 <= received <= total <= 154:
                    raise ValueError('Invalid packet counts')
                self.received, self.total = received, total
                self.rssi = event.get('rssi')
                self.last_event = f'Packets received: {received}/{total}'
            elif kind == 'image':
                if len(event.get('data', '')) > 32768:
                    raise ValueError('Image payload too large')
                image = base64.b64decode(event['data'], validate=True)
                if not 0 < len(image) <= MAX_IMAGE or len(image) != int(event['bytes']):
                    raise ValueError('Image size mismatch')
                if zlib.crc32(image) != int(event['crc32']):
                    raise ValueError('Image CRC32 mismatch')
                if not image.startswith(b'\xff\xd8') or not image.endswith(b'\xff\xd9'):
                    raise ValueError('Invalid JPEG markers')
                self.image, self.image_bytes = image, len(image)
                self.image_version += 1
                self.busy = False
                self.received = self.total
                self.log(f'Image reconstructed: {len(image)} bytes; CRC32 verified.')
            elif kind == 'error':
                self.busy = False
                self.log('Error: ' + str(event.get('message', 'Unknown error')))
            elif kind == 'status':
                message = str(event.get('message', ''))
                if message == 'Ground station ready':
                    self.busy = False
                self.log(message)

    def read_loop(self, connection, stop):
        buffer = bytearray()
        try:
            while not stop.is_set():
                chunk = connection.read(512)
                if chunk:
                    buffer.extend(chunk)
                    if len(buffer) > 70000:
                        buffer.clear()
                        self.log('Discarded oversized serial line.')
                    while b'\n' in buffer:
                        line, _, remainder = buffer.partition(b'\n')
                        buffer = bytearray(remainder)
                        if not line.startswith(b'{'):
                            continue  # ESP boot messages are not application JSON.
                        try:
                            self.ingest(json.loads(line))
                        except (ValueError, TypeError, KeyError, binascii.Error):
                            self.log('Discarded malformed or corrupt serial data.')
                with self.lock:
                    if self.busy and time.monotonic() - self.command_at > 300:
                        self.busy = False
                        self.log('Transfer exceeded five minutes. Check the remote node.')
        except (serial.SerialException, OSError):
            if not stop.is_set():
                self.log('USB serial disconnected.')
        finally:
            with self.lock:
                if self.connection is connection:
                    self.connection = None
                    self.busy = False
                    self.port = ''
            try:
                connection.close()
            except serial.SerialException:
                pass

    def snapshot(self):
        with self.lock:
            return {'connected': bool(self.connection and self.connection.is_open), 'port': self.port,
                    'busy': self.busy, 'received': self.received, 'total': self.total, 'rssi': self.rssi,
                    'message': self.last_event, 'samples': list(self.samples), 'logs': list(self.logs),
                    'image_version': self.image_version, 'image_bytes': self.image_bytes}


def create_app(station=None):
    app = Flask(__name__, root_path=str(Path(__file__).resolve().parent))
    app.config['MAX_CONTENT_LENGTH'] = 4096
    app.config['CONTROL_TOKEN'] = secrets.token_hex(24)
    station = station or Station()
    app.config['STATION'] = station

    @app.before_request
    def local_controls():
        if request.method == 'POST' and request.headers.get('X-Control-Token') != app.config['CONTROL_TOKEN']:
            return jsonify(error='Reload the dashboard before using controls.'), 403

    @app.get('/')
    def index():
        return render_template('index.html', control_token=app.config['CONTROL_TOKEN'])

    @app.get('/api/ports')
    def ports():
        return jsonify([{'device': p.device, 'description': p.description} for p in list_ports.comports()])

    @app.get('/api/state')
    def state():
        return jsonify(station.snapshot())

    @app.post('/api/connect')
    def connect():
        port = (request.get_json(silent=True) or {}).get('port')
        if port not in {p.device for p in list_ports.comports()}:
            return jsonify(error='Select an available serial port.'), 400
        try:
            station.connect(port)
        except (serial.SerialException, OSError, ValueError):
            return jsonify(error='Cannot open port. Close Arduino Serial Monitor and try again.'), 400
        return jsonify(ok=True)

    @app.post('/api/disconnect')
    def disconnect():
        station.disconnect()
        return jsonify(ok=True)

    @app.post('/api/command')
    def command():
        name = (request.get_json(silent=True) or {}).get('command')
        if name not in ('SENSOR', 'PHOTO'):
            return jsonify(error='Unknown command'), 400
        try:
            station.command(name)
        except (ValueError, serial.SerialException, OSError) as exc:
            return jsonify(error=str(exc)), 409
        return jsonify(ok=True)

    @app.get('/image.jpg')
    def image():
        with station.lock:
            image = station.image
        if image is None:
            return jsonify(error='No image received yet'), 404
        response = send_file(io.BytesIO(image), mimetype='image/jpeg', download_name='lora-capture.jpg')
        response.headers['Cache-Control'] = 'no-store'
        return response

    return app

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port', type=int, default=5000, help='Local dashboard HTTP port')
    args = parser.parse_args()
    create_app().run(host='127.0.0.1', port=args.port, debug=False, use_reloader=False, threaded=True)
