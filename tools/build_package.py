#!/usr/bin/env python3
"""Build the exact standard package reproducibly; generated archives are ignored."""
from pathlib import Path
from zipfile import ZipFile, ZipInfo, ZIP_DEFLATED
import argparse, hashlib, subprocess, sys
ROOT = Path(__file__).resolve().parents[1]
PACK = ROOT / 'packs/paper-mario-pal/standard'
FILES = ('manifest.ini','guide.tsv','hints.tsv','translations-en.tsv','translations-fr.tsv',
         'translations-es.tsv','groups.tsv','images/map.tsv','guide_pal_badge_recipe.json')
def build(output):
    subprocess.run([sys.executable, str(ROOT/'tools/validate_guide_package.py'), str(PACK)], check=True)
    actual={p.relative_to(PACK).as_posix() for p in PACK.rglob('*') if p.is_file()}
    if actual!=set(FILES) or any(p.is_symlink() for p in PACK.rglob('*')): raise ValueError('unexpected package files or symlinks')
    output.parent.mkdir(parents=True,exist_ok=True)
    with output.open('xb') as f:
        with ZipFile(f,'w',ZIP_DEFLATED,compresslevel=9) as z:
            for name in sorted(FILES):
                zi=ZipInfo('standard/'+name,(1980,1,1,0,0,0));zi.create_system=3;zi.external_attr=0o100644<<16;zi.compress_type=ZIP_DEFLATED
                z.writestr(zi,(PACK/name).read_bytes(),compress_type=ZIP_DEFLATED,compresslevel=9)
    data=output.read_bytes()
    print('bytes=',len(data),'sha256=',hashlib.sha256(data).hexdigest())
if __name__=='__main__':
    ap=argparse.ArgumentParser();ap.add_argument('output',type=Path);args=ap.parse_args()
    try:build(args.output)
    except (OSError,ValueError,subprocess.CalledProcessError) as e:print('Build failed:',e,file=sys.stderr);sys.exit(1)
