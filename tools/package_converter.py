#!/usr/bin/env python3
"""Package one locally built ARM PAL converter at its fixed central path."""
from pathlib import Path
from zipfile import ZipFile, ZipInfo, ZIP_DEFLATED
import argparse, hashlib, re, struct, subprocess, sys
DEST='App/UniversalGuide/tools/PaperMarioPAL/guide-pal-rom-converter'
def build(binary, output):
    data=binary.read_bytes()
    if len(data)<52 or data[:6]!=b'\x7fELF\x01\x01' or struct.unpack_from('<HH',data,16)!=(2,40):
        raise ValueError('expected little-endian ELF32 ARM executable')
    flags=struct.unpack_from('<I',data,36)[0]
    if flags&0xff000000 != 0x05000000 or not flags&0x400: raise ValueError('expected EABI5 hard-float')
    dynamic=subprocess.run(['readelf','-d',str(binary)],check=True,text=True,capture_output=True).stdout
    libs=set(re.findall(r'\(NEEDED\).*?\[([^\]]+)\]',dynamic))
    if libs!={'libc.so.6','libm.so.6'} or 'RPATH' in dynamic or 'RUNPATH' in dynamic: raise ValueError('unexpected runtime dependencies or search path')
    output.parent.mkdir(parents=True,exist_ok=True)
    with output.open('xb') as f:
        with ZipFile(f,'w',ZIP_DEFLATED,compresslevel=9) as z:
            zi=ZipInfo(DEST,(1980,1,1,0,0,0));zi.create_system=3;zi.external_attr=0o100755<<16;zi.compress_type=ZIP_DEFLATED;z.writestr(zi,data,compress_type=ZIP_DEFLATED,compresslevel=9)
    archive=output.read_bytes();print('bytes=',len(archive),'sha256=',hashlib.sha256(archive).hexdigest())
if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('binary',type=Path);p.add_argument('output',type=Path);a=p.parse_args()
    try:build(a.binary,a.output)
    except (OSError,ValueError,subprocess.CalledProcessError) as exc: print('Converter packaging failed:',exc,file=sys.stderr);sys.exit(1)
