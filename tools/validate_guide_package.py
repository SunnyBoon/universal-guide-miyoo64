#!/usr/bin/env python3
"""Check one G3.6 guide package; no game assets or dependencies required."""
import csv
import pathlib
import re
import sys

def fail(message):
    raise ValueError(message)

def main(path):
    p = pathlib.Path(path)
    if p.is_symlink() or not p.is_dir() or not re.fullmatch(r'[a-z0-9_-]{1,63}', p.name) or p.name == 'paper-mario-standard':
        fail('invalid package directory or ID')
    m = p / 'manifest.ini'
    if m.is_symlink() or not m.is_file(): fail('manifest must be a regular file')
    data = {}
    for line in m.read_text(encoding='utf-8').splitlines():
        if not line or line.startswith('#'): continue
        key, sep, value = line.partition('=')
        if not sep or key not in ('id', 'name', 'auto', 'name_de', 'name_en', 'name_fr', 'name_es') or key in data: fail('invalid manifest entry')
        data[key] = value
    if data.get('id') != p.name or not data.get('name') or len(data['name'].encode()) >= 128 or any(ord(c)<32 or ord(c)==127 for c in data['name']): fail('invalid manifest ID/name')
    for key in ('name_de','name_en','name_fr','name_es'):
        if key in data and (not data[key] or len(data[key].encode()) >= 128 or any(ord(c)<32 or ord(c)==127 for c in data[key])): fail('invalid localized name')
    if data.get('auto','none') not in ('none','paper-mario-pal-v1'): fail('invalid auto declaration')
    tsv=p/'guide.tsv'
    if tsv.is_symlink() or not tsv.is_file(): fail('guide.tsv must be a regular file')
    rows=list(csv.reader(tsv.open(encoding='utf-8',newline=''),delimiter='\t',quoting=csv.QUOTE_NONE))
    safe=re.compile(r'[A-Za-z0-9_.-]+\Z')
    if not rows or len(rows[0])!=2 or any(not safe.fullmatch(x) or len(x.encode())>=64 for x in rows[0]): fail('invalid TSV header')
    seen=set(); categories=set()
    for row in rows[1:]:
        if not row or row[0].startswith('#'): continue
        if len(row) not in (7,12) or not safe.fullmatch(row[0]) or len(row[0].encode())>=64 or row[0] in seen: fail('invalid or duplicate entry ID')
        seen.add(row[0]); categories.add(row[2])
        limits=[64,128,128,128,512,512,128]+([32,256,32,256,32] if len(row)==12 else [])
        if any(len(v.encode())>=n for v,n in zip(row,limits)): fail('TSV field too long')
    if not seen: fail('empty guide')
    for child in (p/'hints.tsv',p/'images'):
        if child.is_symlink(): fail('optional path must not be a symlink')
    hints=p/'hints.tsv'
    if hints.exists():
        if not hints.is_file(): fail('hints is not a regular file')
        ids=set()
        for row in csv.reader(hints.open(encoding='utf-8',newline=''),delimiter='\t',quoting=csv.QUOTE_NONE):
            if len(row)!=3 or row[0] not in seen or row[0] in ids or not row[1] or not row[2] or len(row[1].encode())>=512 or len(row[2].encode())>=512: fail('invalid or duplicate hint')
            ids.add(row[0])
    groups = p / 'groups.tsv'
    if groups.is_symlink(): fail('groups must not be a symlink')
    if groups.exists():
        if not groups.is_file(): fail('groups is not a regular file')
        memberships = set()
        with groups.open(encoding='utf-8', newline='') as group_file:
            for row in csv.reader(group_file, delimiter='\t', quoting=csv.QUOTE_NONE):
                if not row or row[0].startswith('#') or (len(row)==1 and not row[0]): continue
                if len(row)!=2 or row[0] not in seen or row[1] not in ('merlow','shop','koopa_koot','quizmo') or tuple(row) in memberships:
                    fail('invalid or duplicate group membership')
                memberships.add(tuple(row))
    for lang in ('en', 'fr', 'es'):
        translation = p / ('translations-' + lang + '.tsv')
        if translation.is_symlink(): fail('translation must not be a symlink')
        if not translation.exists(): continue
        if not translation.is_file(): fail('translation is not a regular file')
        ids = set()
        for row in csv.reader(translation.open(encoding='utf-8', newline=''), delimiter='\t', quoting=csv.QUOTE_NONE):
            if not row or row[0].startswith('#'): continue
            if len(row) != 8 or row[0] not in seen or row[0] in ids or not any(row[1:]):
                fail('invalid or duplicate translation ID in ' + translation.name)
            if any(len(value.encode()) >= limit for value, limit in zip(row, (64,128,128,256,512,512,512,512))):
                fail('translation field too long in ' + translation.name)
            ids.add(row[0])
    images=p/'images'
    if images.exists():
        if not images.is_dir(): fail('images is not a directory')
        mapping=images/'map.tsv'
        if mapping.is_symlink(): fail('image map must not be a symlink')
        if mapping.exists():
            if not mapping.is_file(): fail('image map is not a regular file')
            keys=set()
            for row in csv.reader(mapping.open(encoding='utf-8',newline=''),delimiter='\t',quoting=csv.QUOTE_NONE):
                if len(row)!=2 or row[0] in keys or (row[0] not in seen and not (row[0].startswith('!') and row[0][1:] in seen) and not (row[0].startswith('@') and row[0][1:] in categories)) or not re.fullmatch(r'(?:private/)?[A-Za-z0-9_-]+',row[1]): fail('invalid or duplicate image reference')
                keys.add(row[0])
        for child in images.iterdir():
            if child.is_symlink(): fail('image path must not be a symlink')
            if child.name=='map.tsv' and child.is_file(): continue
            if child.name=='private' and child.is_dir():
                for item in child.iterdir():
                    if item.is_symlink() or not item.is_file() or not re.fullmatch(r'[A-Za-z0-9_-]+\.bmp',item.name): fail('invalid private image path')
                continue
            if not child.is_file() or not re.fullmatch(r'[A-Za-z0-9_-]+\.bmp',child.name): fail('invalid image path')
    print(f'OK: {p.name}, {len(seen)} entries, auto={data.get("auto","none")}')

if __name__ == '__main__':
    try:
        if len(sys.argv)!=2: fail('usage: validate_guide_package.py PACKAGE')
        main(sys.argv[1])
    except (OSError, UnicodeError, ValueError) as exc:
        print(f'Invalid package: {exc}', file=sys.stderr); sys.exit(1)
