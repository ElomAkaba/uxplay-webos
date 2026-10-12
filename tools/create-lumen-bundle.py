#!/usr/bin/env python3
"""Bundle a launcher IPK with its hidden native AirPlay backend."""
import argparse,json,pathlib,re,shutil,subprocess,zipfile
ROOT=pathlib.Path(__file__).resolve().parent.parent
parser=argparse.ArgumentParser()
parser.add_argument('--launcher-ipk',required=True,type=pathlib.Path)
parser.add_argument('--launcher-id',required=True)
parser.add_argument('--receiver-name',default='Lumen TV')
args=parser.parse_args()
if not args.launcher_ipk.is_file():parser.error('launcher IPK does not exist')
if not re.fullmatch('[A-Za-z0-9._-]{1,128}',args.launcher_id):parser.error('invalid launcher ID')
if not args.receiver_name or len(args.receiver_name.encode())>60 or any(ord(c)<32 for c in args.receiver_name):parser.error('invalid receiver name')
subprocess.run(['python3',str(ROOT/'tools/package.py')],check=True)
stage=ROOT/'build/lumen-backend'
if stage.exists():shutil.rmtree(stage)
shutil.copytree(ROOT/'build/app',stage)
metadata=stage/'appinfo.json';app=json.loads(metadata.read_text());app['visible']=False;app['title']='Lumen AirPlay';metadata.write_text(json.dumps(app,indent=2)+'\n')
output=ROOT/'build/lumen-ipks';output.mkdir(exist_ok=True)
subprocess.run(['ares-package',str(stage),'-o',str(output)],check=True)
backend=output/f"{app['id']}_{app['version']}_arm.ipk"
archive=ROOT/'dist/lumen-airplay-bundle.zip'
with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED) as bundle:
    bundle.write(args.launcher_ipk,'launcher.ipk')
    bundle.write(backend,'airplay-backend.ipk')
    bundle.writestr('launcher.conf',args.launcher_id+'\n'+args.receiver_name+'\n')
    bundle.writestr('README.txt','Install both IPKs on a rooted webOS TV. The backend is hidden from the system launcher. Copy launcher.conf into the backend private state directory, readable by its app user (mode 0600). Preserve existing pairing state when updating. The launcher must provide an AirPlay action that launches '+app['id']+'. See docs/LUMEN.md in the source repository for setup and tested limitations. The optional HLS audio companion is configured separately.\n')
print(archive)
