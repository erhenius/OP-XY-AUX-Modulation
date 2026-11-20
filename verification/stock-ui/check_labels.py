"""Run the patched and stock SDK LFO destination regression fixtures."""
import argparse
from pathlib import Path
import shutil
import subprocess
import tempfile

args = argparse.ArgumentParser()
args.add_argument('--sdk', type=Path, required=True)
sdk = args.parse_args().sdk.resolve()
platform = sdk / 'sdk/platform/opxy-1.2.1'
with tempfile.TemporaryDirectory() as folder:
    temp = Path(folder)
    original = sdk / 'tests/fixtures/lfo.cpp'
    shutil.copyfile(original, temp / 'lfo.cpp.fixture')
    for selected in (False, True):
        source = Path(__file__).with_name('test_labels.cpp') if selected else original
        command = ['c++', '-std=c++11', '-O2', '-Wall', '-Werror', '-fsanitize=undefined',
                   '-fno-sanitize-recover=all', '-I' + str(temp), '-I' + str(platform),
                   '-I' + str(sdk / 'sdk/include')]
        if selected:
            command.append('-DOPXY_AUX_LFO=1')
        exe = temp / 'labels'
        subprocess.run(command + [str(source), str(platform / 'lfo.cpp'), '-o', str(exe)], check=True)
        subprocess.run([str(exe)], check=True)
        print('PASS' if selected else 'PASS baseline', 'stock LFO destination labels')
