import base64
import importlib.util
from pathlib import Path
import unittest
import zlib
ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('dashboard_app',ROOT/'dashboard/app.py')
module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)

class DashboardTests(unittest.TestCase):
 def setUp(self):
  self.station=module.Station();self.app=module.create_app(self.station);self.client=self.app.test_client();self.headers={'X-Control-Token':self.app.config['CONTROL_TOKEN']}
 def test_sensor_history(self):
  self.station.ingest({'type':'sensor','values':{'temperature':27.5,'humidity':62},'rssi':-88})
  state=self.client.get('/api/state').get_json();self.assertEqual(state['samples'][0]['temperature'],27.5);self.assertEqual(state['rssi'],-88)
 def test_invalid_reading(self):
  with self.assertRaises(ValueError):self.station.ingest({'type':'sensor','values':{'temperature':float('nan'),'humidity':55}})
 def test_image_validation(self):
  # Minimal marker fixture tests transport validation, not JPEG decoder correctness.
  data=b'\xff\xd8test-data\xff\xd9';event={'type':'image','bytes':len(data),'crc32':zlib.crc32(data),'data':base64.b64encode(data).decode()}
  self.station.ingest(event);self.assertEqual(self.client.get('/image.jpg').data,data)
  event['crc32']^=1
  with self.assertRaises(ValueError):self.station.ingest(event)
 def test_ui_and_local_controls(self):
  response=self.client.get('/');self.assertEqual(response.status_code,200);self.assertIn(b'Send picture',response.data)
  self.assertEqual(self.client.post('/api/disconnect',json={}).status_code,403)
  self.assertEqual(self.client.post('/api/command',json={'command':'BAD'},headers=self.headers).status_code,400)
  self.assertEqual(self.client.post('/api/command',json={'command':'PHOTO'},headers=self.headers).status_code,409)
 def test_progress_and_serial_noise(self):
  self.station.ingest({'type':'progress','received':4,'total':10,'rssi':-92});self.assertEqual(self.station.snapshot()['received'],4)
  with self.assertRaises(ValueError):self.station.ingest({'type':'progress','received':11,'total':10})
  self.station.ingest(['not','an','event'])
 def test_identical_shared_headers(self):
  for name in ('Protocol.h','RadioLink.h'):
   self.assertEqual((ROOT/'firmware/RemoteNode'/name).read_bytes(),(ROOT/'firmware/GroundStation'/name).read_bytes())


class SerialReaderTests(unittest.TestCase):
 def test_fragmented_image_and_boot_noise(self):
  import json
  import threading
  class FakeSerial:
   def __init__(self,wire,stop):self.wire=wire;self.stop=stop
   def read(self,count):
    if not self.wire:self.stop.set();return b''
    out,self.wire=self.wire[:37],self.wire[37:];return out
   def close(self):pass
  station=module.Station();stop=threading.Event()
  data=b'\xff\xd8'+bytes(range(256))*40+b'\xff\xd9'
  event={'type':'image','bytes':len(data),'crc32':zlib.crc32(data),'data':base64.b64encode(data).decode()}
  wire=b'boot text\n{broken json}\n'+json.dumps(event).encode()+b'\n'
  station.read_loop(FakeSerial(wire,stop),stop)
  self.assertEqual(station.image,data)
  self.assertEqual(station.image_version,1)

if __name__ == "__main__":
 unittest.main()
