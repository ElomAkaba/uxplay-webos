#!/usr/bin/env python3
import argparse,http.server,ipaddress,json,os,pathlib,re,secrets,subprocess,threading,time,urllib.request
from urllib.parse import urlsplit
os.umask(0o077)
parser=argparse.ArgumentParser(description='Combine YouTube AirPlay HLS video and audio for older webOS TVs')
parser.add_argument('--config',required=True)
args=parser.parse_args()
config=json.loads(pathlib.Path(args.config).read_text())
TV=str(ipaddress.IPv4Address(config['tv_host']))
BIND=str(ipaddress.IPv4Address(config.get('bind','127.0.0.1')))
PORT=int(config.get('port',8878))
if not 1024<=PORT<=65535:raise ValueError('Invalid port')
PUBLIC=config['public_url'].rstrip('/')
parsed=urlsplit(PUBLIC)
if parsed.scheme!='http' or parsed.hostname!=BIND or parsed.port!=PORT or parsed.path or parsed.username:raise ValueError('public_url must match bind and port')
ROOT=pathlib.Path(config['state_dir']).resolve()
if len(ROOT.parts)<3:raise ValueError('Use a dedicated state directory')
ROOT.mkdir(parents=True,exist_ok=True)
TOKEN=pathlib.Path(config['token_file']).read_text().strip()
if not re.fullmatch('[0-9a-fA-F]{64}',TOKEN):raise ValueError('Token must contain 64 hexadecimal characters')
lock=threading.Lock();process=None

def fetch(url):
    with urllib.request.urlopen(url,timeout=10) as r:return r.read().decode()
def prepare(port):
    global process
    with lock:
        if process and process.poll() is None:
            process.terminate()
            try:process.wait(timeout=5)
            except subprocess.TimeoutExpired:process.kill();process.wait()
        for old in ROOT.iterdir():
            if old.is_dir():
                import shutil;shutil.rmtree(old)
        base=f'http://{TV}:{port}'
        master=fetch(base+'/master.m3u8');lines=master.splitlines();audio={};variants=[]
        for n,line in enumerate(lines):
            if line.startswith('#EXT-X-MEDIA:') and 'TYPE=AUDIO' in line:
                group=re.search(r'GROUP-ID="([^"]+)"',line);uri=re.search(r'URI="([^"]+)"',line)
                if group and uri:audio[group[1]]=uri[1]
            if line.startswith('#EXT-X-STREAM-INF:') and 'avc1.' in line:
                res=re.search(r'RESOLUTION=(\d+)x(\d+)',line);group=re.search(r'AUDIO="([^"]+)"',line)
                if res and group and int(res[1])<=1280 and int(res[2])<=720 and n+1<len(lines):
                    variants.append(('mp4a.40.2' in line,int(res[1]),lines[n+1],audio.get(group[1])))
        if not variants:raise ValueError('No compatible video/audio variant')
        _,_,video_uri,audio_uri=max(variants)
        if not audio_uri:raise ValueError('No separate audio playlist')
        def local(uri):
            parsed=urlsplit(uri)
            if parsed.scheme!='http' or parsed.hostname not in ('localhost','127.0.0.1',TV) or parsed.port!=port or not parsed.path.startswith('/itag/') or parsed.query or parsed.username:
                raise ValueError('Unexpected proxy playlist origin')
            return base+parsed.path
        sid=secrets.token_hex(16);folder=ROOT/sid;folder.mkdir()
        for name,uri in [('video',video_uri),('audio',audio_uri)]:
            playlist=fetch(local(uri))
            for line in playlist.splitlines():
                if line.startswith('#EXT-X-KEY:'):raise ValueError('Encrypted media is not supported')
                embedded=re.search(r'URI="([^"]+)"',line)
                if (line and not line.startswith('#')) or embedded:
                    segment=urlsplit(embedded[1] if embedded else line)
                    if segment.scheme!='https' or not segment.hostname or not segment.hostname.endswith('.googlevideo.com'):
                        raise ValueError('Unexpected media segment origin')
            (folder/(name+'.m3u8')).write_text(playlist)
        log=open(folder/'ffmpeg.log','wb')
        process=subprocess.Popen(['ffmpeg','-hide_banner','-loglevel','warning','-nostdin','-protocol_whitelist','file,http,https,tcp,tls,crypto','-allowed_extensions','ALL','-allowed_segment_extensions','ALL','-extension_picky','0','-i',str(folder/'video.m3u8'),'-protocol_whitelist','file,http,https,tcp,tls,crypto','-allowed_extensions','ALL','-allowed_segment_extensions','ALL','-extension_picky','0','-i',str(folder/'audio.m3u8'),'-map','0:v:0','-map','1:a:0','-c','copy','-f','hls','-hls_time','4','-hls_list_size','0','-hls_playlist_type','event','-hls_flags','independent_segments','-hls_segment_filename',str(folder/'part%05d.ts'),str(folder/'index.m3u8')],stdout=subprocess.DEVNULL,stderr=log)
        log.close();deadline=time.monotonic()+25
        while time.monotonic()<deadline:
            if (folder/'index.m3u8').exists() and len(list(folder.glob('part*.ts')))>=2:return f'{PUBLIC}/{sid}/index.m3u8'
            if process.poll() is not None:raise ValueError('Video/audio merge failed; inspect ffmpeg.log')
            time.sleep(.2)
        process.terminate();raise ValueError('Video/audio merge timed out')

class Handler(http.server.SimpleHTTPRequestHandler):
    def __init__(self,*args,**kwargs):super().__init__(*args,directory=str(ROOT),**kwargs)
    def log_message(self,*args):pass
    def do_GET(self):
        if self.path.startswith('/start?'):
            if not secrets.compare_digest(self.headers.get('Authorization',''), 'Bearer '+TOKEN):self.send_error(403);return
            from urllib.parse import parse_qs,urlsplit
            try:
                port=int(parse_qs(urlsplit(self.path).query)['port'][0])
                if not 1024<=port<=65535:raise ValueError('Invalid source port')
                result=prepare(port).encode();self.send_response(200);self.send_header('Content-Type','text/plain');self.send_header('Content-Length',str(len(result)));self.end_headers();self.wfile.write(result)
            except Exception as e:
                print(type(e).__name__+': stream preparation failed',flush=True);self.send_error(502,'Stream merge unavailable')
        elif re.fullmatch(r'/[0-9a-f]{32}/(?:index\.m3u8|part\d+\.ts)',self.path):super().do_GET()
        else:self.send_error(404)

http.server.ThreadingHTTPServer((BIND,PORT),Handler).serve_forever()
