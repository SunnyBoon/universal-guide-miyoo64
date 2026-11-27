#!/usr/bin/env python3
"""Exercise catalog, archive validation and isolated standard-guide updates."""
from pathlib import Path
from zipfile import ZipFile, ZIP_DEFLATED, BadZipFile
import copy, hashlib, json, tempfile, unittest, struct, subprocess
from unittest import mock
from urllib.request import Request
from urllib.error import HTTPError
from build_package import build
from install_host import install, checked_files

ROOT=Path(__file__).resolve().parents[1]
class DistributionTest(unittest.TestCase):
    def setUp(self):
        self.t=tempfile.TemporaryDirectory(prefix='g313-distribution-');self.addCleanup(self.t.cleanup);self.p=Path(self.t.name)
        self.zip=self.p/'guide.zip';build(self.zip)
        self.catalog=self.p/'catalog.json';self.c=json.loads((ROOT/'catalog.json').read_text());self.write_catalog()
        self.target=self.p/'guides';self.target.mkdir();self.progress=self.p/'guide-progress.txt';self.progress.write_bytes(b'manual progress')
        self.settings=self.p/'PaperMarioPAL.ini';self.settings.write_bytes(b'filter settings')
    def write_catalog(self):self.catalog.write_text(json.dumps(self.c))
    def apply(self):install(self.catalog,'paper-mario-pal-standard',self.target,True,self.zip.as_uri())
    def test_reproducible_install_update_and_failure(self):
        other=self.p/'second.zip';build(other);self.assertEqual(self.zip.read_bytes(),other.read_bytes())
        self.apply();dest=self.target/'standard';self.assertEqual((dest/'guide.tsv').read_bytes(),(ROOT/'packs/paper-mario-pal/standard/guide.tsv').read_bytes())
        img=dest/'images/private/user.bmp';img.parent.mkdir();pixel=b'\x00\x00\x00\x00'; bmp=b'BM'+struct.pack('<IHHI',58,0,0,54)+struct.pack('<IiiHHIIiiII',40,1,1,1,24,0,4,0,0,0,0)+pixel;img.write_bytes(bmp);self.apply();self.assertEqual(img.read_bytes(),bmp)
        before=(dest/'guide.tsv').read_bytes();self.c['packages'][0]['sha256']='0'*64;self.write_catalog()
        with self.assertRaises(ValueError):self.apply()
        self.assertEqual((dest/'guide.tsv').read_bytes(),before);self.assertEqual(img.read_bytes(),bmp);self.assertEqual(self.progress.read_bytes(),b'manual progress');self.assertEqual(self.settings.read_bytes(),b'filter settings')
    def test_failed_final_move_restores_previous_package(self):
        self.apply();dest=self.target/'standard';before=(dest/'guide.tsv').read_bytes()
        import install_host
        original=install_host.os.rename; calls=[]
        def fail_second(src,dst):
            calls.append((src,dst))
            if len(calls)==2: raise OSError('simulated final move failure')
            return original(src,dst)
        with mock.patch.object(install_host.os,'rename',side_effect=fail_second):
            with self.assertRaises(OSError):self.apply()
        self.assertEqual((dest/'guide.tsv').read_bytes(),before);self.assertEqual(len(calls),3)
        self.assertEqual(self.progress.read_bytes(),b'manual progress')
    def test_truncated_download_preserves_existing_package(self):
        self.apply();dest=self.target/'standard';before=(dest/'guide.tsv').read_bytes()
        self.zip.write_bytes(self.zip.read_bytes()[:-1])
        with self.assertRaises(ValueError):self.apply()
        self.assertEqual((dest/'guide.tsv').read_bytes(),before)
        self.assertEqual(self.progress.read_bytes(),b'manual progress')
        self.assertEqual(self.settings.read_bytes(),b'filter settings')

    def test_https_catalog_read_and_http_catalog_rejected(self):
        import install_host
        catalog_url='https://example.test/catalog.json'
        real_fetch=install_host.fetch
        def fake_fetch(url,local_test,limit=install_host.MAX_ARCHIVE):
            if url==catalog_url:
                self.assertFalse(local_test);self.assertEqual(limit,install_host.MAX_CATALOG)
                return json.dumps(self.c).encode()
            return real_fetch(url,local_test,limit)
        with mock.patch.object(install_host,'fetch',side_effect=fake_fetch):
            install(catalog_url,'paper-mario-pal-standard',self.target,True,self.zip.as_uri())
        with self.assertRaises(ValueError):install('http://example.test/catalog.json','paper-mario-pal-standard',self.target)

    def test_https_errors_and_redirect_downgrade(self):
        import install_host
        with self.assertRaises(ValueError):
            install_host.HTTPSRedirect().redirect_request(Request('https://example.test/a'),None,302,'redirect',{},'http://example.test/b')
        opener=mock.Mock()
        opener.open.side_effect=HTTPError('https://example.test/a',404,'not found',{},None)
        with mock.patch.object(install_host,'build_opener',return_value=opener):
            with self.assertRaises(HTTPError):install_host.fetch('https://example.test/a',False)
        request=opener.open.call_args.args[0]
        self.assertNotIn('authorization',{k.lower() for k,v in request.header_items()})
        self.assertNotIn('cookie',{k.lower() for k,v in request.header_items()})

    def test_incomplete_https_response_is_rejected(self):
        import install_host
        response=mock.MagicMock()
        response.geturl.return_value='https://example.test/a'
        response.__enter__.return_value=response
        response.headers={'Content-Length':'100'}
        response.read.return_value=b'short'
        opener=mock.Mock();opener.open.return_value=response
        with mock.patch.object(install_host,'build_opener',return_value=opener):
            with self.assertRaises(ValueError):install_host.fetch('https://example.test/a',False)

    def test_archive_traversal_rejected_after_valid_hash(self):
        self.apply();dest=self.target/'standard';before=(dest/'guide.tsv').read_bytes()
        with ZipFile(self.zip,'a',ZIP_DEFLATED) as z:z.writestr('../escape',b'bad')
        d=self.zip.read_bytes();self.c['packages'][0]['size']=len(d);self.c['packages'][0]['sha256']=hashlib.sha256(d).hexdigest();self.write_catalog()
        with self.assertRaises(ValueError):self.apply()
        self.assertEqual((dest/'guide.tsv').read_bytes(),before);self.assertFalse((self.p/'escape').exists())
    def test_invalid_guide_with_matching_hash_preserves_installation(self):
        self.apply();dest=self.target/'standard';before=(dest/'guide.tsv').read_bytes()
        with ZipFile(self.zip) as z: files={name:z.read(name) for name in z.namelist()}
        lines=files['standard/guide.tsv'].splitlines(keepends=True)
        files['standard/guide.tsv']+=lines[1]
        with ZipFile(self.zip,'w',ZIP_DEFLATED) as z:
            for name,data in files.items():z.writestr(name,data)
        d=self.zip.read_bytes();self.c['packages'][0]['size']=len(d);self.c['packages'][0]['sha256']=hashlib.sha256(d).hexdigest();self.write_catalog()
        with self.assertRaises(subprocess.CalledProcessError):self.apply()
        self.assertEqual((dest/'guide.tsv').read_bytes(),before)
        self.assertEqual(self.progress.read_bytes(),b'manual progress')

    def test_corrupt_archive_with_matching_hash_preserves_installation(self):
        self.apply();dest=self.target/'standard';before=(dest/'guide.tsv').read_bytes()
        self.zip.write_bytes(b'not a zip');d=self.zip.read_bytes()
        self.c['packages'][0]['size']=len(d);self.c['packages'][0]['sha256']=hashlib.sha256(d).hexdigest();self.write_catalog()
        with self.assertRaises(BadZipFile):self.apply()
        self.assertEqual((dest/'guide.tsv').read_bytes(),before)

    def test_symlink_existing_images_fails_closed(self):
        self.apply();dest=self.target/'standard';(dest/'images'/'malicious').symlink_to(self.progress)
        with self.assertRaises(ValueError):self.apply()
        self.assertEqual(self.progress.read_bytes(),b'manual progress')
if __name__=='__main__':unittest.main()
