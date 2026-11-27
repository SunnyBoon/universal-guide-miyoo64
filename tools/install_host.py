#!/usr/bin/env python3
"""Isolated host-only catalog downloader and transactional standard-guide installer."""
from pathlib import Path, PurePosixPath
from zipfile import ZipFile, BadZipFile
from urllib.parse import urlsplit, unquote
from urllib.request import Request, build_opener, HTTPRedirectHandler
import argparse, fcntl, hashlib, json, os, re, shutil, stat, subprocess, sys, tempfile
from http.client import HTTPException
from build_package import FILES
ROOT=Path(__file__).resolve().parents[1]
MAX_ARCHIVE=4*1024*1024
MAX_CONTENT=2*1024*1024
MAX_CATALOG=256*1024

class HTTPSRedirect(HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        target=urlsplit(newurl)
        if target.scheme!='https' or not target.netloc or target.username or target.password:
            raise ValueError('redirect must remain on HTTPS without credentials')
        return super().redirect_request(req,fp,code,msg,headers,newurl)

def fetch(url, local_test, limit=MAX_ARCHIVE):
    u=urlsplit(url)
    if u.scheme=='file' and local_test and u.netloc in ('','localhost'):
        p=Path(unquote(u.path)).resolve(strict=True)
        if not p.is_relative_to(Path(tempfile.gettempdir()).resolve()) or not p.is_file(): raise ValueError('local test URL must point under temp')
        f=p.open('rb')
    elif u.scheme=='https' and u.netloc and not u.username and not u.password:
        f=build_opener(HTTPSRedirect()).open(Request(url,headers={'User-Agent':'universal-guide-host/1'}),timeout=30)
        final=urlsplit(f.geturl())
        if final.scheme!='https' or not final.netloc or final.username or final.password: f.close(); raise ValueError('HTTPS redirect required')
    else: raise ValueError('URL must be HTTPS (file URLs require --local-test)')
    with f:
        declared=f.headers.get('Content-Length') if hasattr(f,'headers') else None
        if declared is not None and (not declared.isdigit() or int(declared)>limit): raise ValueError('invalid or excessive Content-Length')
        data=f.read(limit+1)
    if len(data)>limit: raise ValueError('download exceeds limit')
    if declared is not None and len(data)!=int(declared): raise ValueError('incomplete download')
    return data

def checked_files(archive):
    expected={'standard/'+x for x in FILES}; seen=set(); total=0; result={}
    with ZipFile(archive) as z:
        for entry in z.infolist():
            name=entry.filename; mode=entry.external_attr>>16; path=PurePosixPath(name)
            if name in seen or name not in expected or path.is_absolute() or '..' in path.parts or '\\' in name or entry.is_dir() or stat.S_ISLNK(mode): raise ValueError('invalid or duplicate archive path')
            if entry.flag_bits&1: raise ValueError('encrypted archive not supported')
            seen.add(name);total+=entry.file_size
            if total>MAX_CONTENT or entry.file_size>MAX_CONTENT: raise ValueError('unpacked content exceeds limit')
            result[name]=z.read(entry)
        if seen!=expected: raise ValueError('package file set incomplete')
    return result

def install(catalog, package_id, root, local_test=False, override=None):
    catalog_url=urlsplit(str(catalog))
    if catalog_url.scheme=='https':
        cat=json.loads(fetch(str(catalog),False,MAX_CATALOG).decode('utf8'))
    elif not catalog_url.scheme:
        cat=json.loads(Path(catalog).read_text(encoding='utf8'))
    else: raise ValueError('catalog must be a local path or HTTPS URL')
    if cat.get('format_version')!=1 or not isinstance(cat.get('packages'),list): raise ValueError('unsupported catalog')
    matches=[x for x in cat['packages'] if x.get('id')==package_id]
    if len(matches)!=1: raise ValueError('catalog ID absent or duplicated')
    entry=matches[0]
    if package_id!='paper-mario-pal-standard' or entry.get('port')!='paper-mario-pal' or entry.get('package_format')!=1: raise ValueError('unsupported package')
    if not all(isinstance(entry.get(k),str) and entry[k].strip() for k in ('name','version','url')): raise ValueError('missing catalog metadata')
    if type(entry.get('size')) is not int or not 0<entry['size']<=MAX_ARCHIVE or not isinstance(entry.get('sha256'),str) or not re.fullmatch(r'[0-9a-f]{64}',entry['sha256']): raise ValueError('invalid size or digest')
    root=Path(root).resolve(strict=True); temp=Path(tempfile.gettempdir()).resolve()
    if root==temp or not root.is_relative_to(temp) or not root.is_dir(): raise ValueError('prototype installs only into an existing temp directory')
    data=fetch(override if override else entry['url'],local_test)
    if len(data)!=entry['size'] or hashlib.sha256(data).hexdigest()!=entry['sha256']: raise ValueError('download size or SHA-256 mismatch')
    import io
    files=checked_files(io.BytesIO(data))
    with tempfile.TemporaryDirectory(prefix='.guide-stage-',dir=root) as td:
        stage=Path(td)/'standard';stage.mkdir()
        for name,content in files.items():
            p=stage/Path(name).relative_to('standard');p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(content)
        subprocess.run([sys.executable,str(ROOT/'tools/validate_guide_package.py'),str(stage)],check=True,stdout=subprocess.DEVNULL)
        if hashlib.sha256((stage/'guide_pal_badge_recipe.json').read_bytes()).hexdigest()!='813c55f8d96dbfd1af0d69f7f0f7e933126bbe12f344bca834cd1464b90e7715': raise ValueError('incompatible image recipe')
        lock=os.open(root,os.O_RDONLY|os.O_DIRECTORY)
        try:
            fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
            dest=root/'standard'
            if dest.is_symlink() or (dest.exists() and not dest.is_dir()): raise ValueError('unsafe existing package')
            if dest.exists():
                for p in dest.rglob('*'):
                    if p.is_symlink() or not (p.is_file() or p.is_dir()): raise ValueError('existing package has unsafe entries')
                    if p.is_dir() and p.relative_to(dest).as_posix()!='images' and not p.relative_to(dest).as_posix().startswith('images/'): raise ValueError('unrecognized existing directory; preserve it manually')
                    if p.is_file() and not p.relative_to(dest).as_posix().startswith('images/') and p.relative_to(dest).as_posix() not in FILES: raise ValueError('unrecognized existing file; preserve it manually')
                old_images=dest/'images'
                if old_images.exists():
                    for p in old_images.rglob('*'):
                        if p.is_file() and p.relative_to(old_images).as_posix()!='map.tsv':
                            q=stage/'images'/p.relative_to(old_images);q.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(p,q)
            backup=None
            if dest.exists():
                backup=Path(tempfile.mkdtemp(prefix='.guide-backup-',dir=root))
                backup.rmdir()
                os.rename(dest,backup)
            try:
                os.rename(stage,dest)
            except BaseException:
                if backup is not None and backup.exists():
                    try: os.rename(backup,dest)
                    except OSError as exc:
                        raise RuntimeError(f'rollback failed; original retained at {backup}') from exc
                raise
            if backup is not None: shutil.rmtree(backup)
        finally:
            os.close(lock)
    print('Installed verified standard guide:',dest)

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--catalog',required=True);p.add_argument('--package',default='paper-mario-pal-standard');p.add_argument('--root',required=True);p.add_argument('--local-test',action='store_true');p.add_argument('--test-url',help='local test override; requires --local-test');a=p.parse_args()
    try:
        if a.test_url and not a.local_test: raise ValueError('test URL requires --local-test')
        install(a.catalog,a.package,a.root,a.local_test,a.test_url)
    except (OSError,ValueError,KeyError,TypeError,RuntimeError,HTTPException,BadZipFile,subprocess.CalledProcessError) as exc:
        print('Installation failed:',exc,file=sys.stderr);sys.exit(1)
