#!/usr/bin/env python3
"""Package corresponding source and original dependency sources; exclude device data."""
import json
import pathlib
import subprocess
import tarfile

ROOT = pathlib.Path(__file__).resolve().parent.parent

def main():
    lock=json.loads((ROOT/'dependencies.lock.json').read_text())
    version=json.loads((ROOT/'packaging/appinfo.json').read_text())['version']
    destination=ROOT/'dist'/f'uxplay-webos-{version}-source.tar.gz'
    destination.parent.mkdir(exist_ok=True)
    subprocess.run(['python3',str(ROOT/'tools/bootstrap.py')],check=True)
    # A clean archive of the exact upstream revision, without Git metadata.
    upstream=ROOT/'.cache/uxplay-source.tar'
    with upstream.open('wb') as out:
        subprocess.run(['git','-C',str(ROOT/'vendor/uxplay'),'archive','--format=tar',
                        '--prefix=uxplay/',lock['uxplay']['commit']],stdout=out,check=True)
    with tarfile.open(destination,'w:gz') as archive:
        for name in ['src','patches','companion','tools','tests','packaging','docs','CMakeLists.txt','dependencies.lock.json','README.md','CHANGELOG.md','THIRD_PARTY.md','LICENSE','.github']:
            archive.add(ROOT/name,arcname='uxplay-webos/'+name,filter=lambda info:None if '__pycache__' in info.name or info.name.endswith('.pyc') else info)
        for name in ['openssl','libplist','ffmpeg']:
            filename=lock[name]['archive']
            archive.add(ROOT/'.cache'/filename,arcname='uxplay-webos/.cache/'+filename)
        archive.add(upstream,arcname='uxplay-webos/upstream-uxplay-source.tar')
    print(destination)

if __name__=='__main__':
    main()
