#!/usr/bin/env python3
import http.server,json,pathlib,socket,subprocess,sys,tempfile,threading,time,urllib.error,urllib.request
class TV(http.server.BaseHTTPRequestHandler):
    def do_GET(self):
        self.send_response(200);self.end_headers();self.wfile.write(b'#EXTM3U\n')
    def log_message(self,*args):pass
with tempfile.TemporaryDirectory() as folder:
    root=pathlib.Path(folder)
    token=root/'token';token.write_text('a'*64)
    sock=socket.socket();sock.bind(('127.0.0.1',0));port=sock.getsockname()[1];sock.close()
    tv=http.server.HTTPServer(('127.0.0.1',0),TV);threading.Thread(target=tv.serve_forever,daemon=True).start()
    config=root/'config.json';config.write_text(json.dumps({'tv_host':'127.0.0.1','bind':'127.0.0.1','port':port,'public_url':f'http://127.0.0.1:{port}','token_file':str(token),'state_dir':str(root/'output')}))
    process=subprocess.Popen([sys.executable,'companion/server.py','--config',str(config)],stdout=subprocess.PIPE,stderr=subprocess.PIPE)
    try:
        for _ in range(100):
            try:
                with socket.create_connection(('127.0.0.1',port),timeout=.1):break
            except OSError:time.sleep(.03)
        def status(path,authorized=False):
            request=urllib.request.Request(f'http://127.0.0.1:{port}{path}',headers={'Authorization':'Bearer '+'a'*64} if authorized else {})
            try:
                with urllib.request.urlopen(request,timeout=3) as response:return response.status
            except urllib.error.HTTPError as error:return error.code
        assert status('/start?port=4321')==403
        assert status('/start?port=0',True)==502
        assert status(f'/start?port={tv.server_port}',True)==502
        assert status('/../token')==404
        assert status('/config.json')==404
    finally:
        process.terminate();process.communicate(timeout=5);tv.shutdown()
print('Companion authorization, port validation and file isolation: PASS')
