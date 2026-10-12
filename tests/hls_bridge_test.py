#!/usr/bin/env python3
import http.server,pathlib,subprocess,sys,tempfile,threading
class Handler(http.server.BaseHTTPRequestHandler):
    response=None
    def do_GET(self):
        assert self.path=='/start?port=4321'
        assert self.headers['Authorization']=='Bearer '+'a'*64
        body=self.response
        self.send_response(200);self.end_headers();self.wfile.write(body)
    def log_message(self,*args):pass
server=http.server.HTTPServer(('127.0.0.1',0),Handler)
threading.Thread(target=server.serve_forever,daemon=True).start()
with tempfile.TemporaryDirectory() as folder:
    config=pathlib.Path(folder)/'bridge.conf';base=f'http://127.0.0.1:{server.server_port}/'
    config.write_text(f'127.0.0.1\n{server.server_port}\n'+ 'a'*64+'\n')
    def call(uri='http://localhost:4321/master.m3u8',path=config):
        return subprocess.check_output([sys.argv[1],uri,str(path)],text=True)
    Handler.response=(base+'abc/index.m3u8').encode();assert call()==base+'abc/index.m3u8'
    Handler.response=b'http://example.com/redirect';assert call()==''
    Handler.response=(base+'x\r\nInjected: true').encode();assert call()==''
    Handler.response=b'x'*10000;assert call()==''
    assert call('http://untrusted.example:4321/master.m3u8')==''
    assert call('http://localhost:0/master.m3u8')==''
    config.write_text('127.0.0.1\n8878\ninvalid-token\n');assert call()==''
    assert call(path=pathlib.Path(folder)/'absent')=='http://localhost:4321/master.m3u8'
server.shutdown();print('HLS bridge configuration and HTTP validation: PASS')
