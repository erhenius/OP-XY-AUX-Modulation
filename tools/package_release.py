"""Package the extension using the checksummed recipe at the repository root."""
import argparse
import hashlib
import io
import json
from pathlib import Path
from zipfile import ZIP_DEFLATED, ZipFile, ZipInfo

ROOT = Path(__file__).resolve().parents[1]


def bundle():
    manifest = ROOT / 'extension-bundle.json'
    recipe = json.loads(manifest.read_text())
    paths = [manifest, *(ROOT / n for n in ('README.md', 'CHANGELOG.md', 'LICENSE', 'docs/BUILD.md',
                                            'LICENSES/OPEN-XY-LICENSE', 'tools/package_release.py'))]
    for patch in recipe['patches']:
        path = ROOT / patch['path']
        if hashlib.sha256(path.read_bytes()).hexdigest() != patch['sha256']:
            raise ValueError('Patch checksum mismatch for ' + patch['path'])
        paths.append(path)
    for folder in (ROOT / recipe['extension'], ROOT / 'verification'):
        paths.extend(p for p in folder.rglob('*') if p.is_file() and p.suffix in {'.cpp','.hpp','.json','.py','.md'})
    result = io.BytesIO()
    with ZipFile(result, 'w') as archive:
        for path in sorted(paths):
            entry = ZipInfo(path.relative_to(ROOT).as_posix())
            entry.create_system = 3
            entry.external_attr = 0o100644 << 16
            archive.writestr(entry, path.read_bytes(), compress_type=ZIP_DEFLATED)
    return ROOT / 'release' / ('AUX-Modulation-' + recipe['version'] + '.zip'), result.getvalue()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    path, data = bundle()
    if args.check:
        with ZipFile(path) as current, ZipFile(io.BytesIO(data)) as expected:
            if current.namelist() != expected.namelist() or any(
                current.read(n) != expected.read(n) for n in expected.namelist()
            ):
                raise SystemExit('Release ZIP differs from the source tree')
    else:
        path.parent.mkdir(exist_ok=True)
        path.write_bytes(data)
    print(path.name)


if __name__ == '__main__':
    main()
