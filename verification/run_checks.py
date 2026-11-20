"""Run the verification sources included in the standalone AUX modulation bundle."""
import argparse
import os
from pathlib import Path
import subprocess
import sys
import tempfile

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sdk",required=True,type=Path,help="patched Open-XY 0.0.7 SDK")
    parser.add_argument("--stock",required=True,type=Path,help="local stock OS 1.2.1 image")
    args=parser.parse_args()
    sdk=args.sdk.resolve()
    stock=args.stock.resolve()
    tests=Path(__file__).resolve().parent
    with tempfile.TemporaryDirectory() as work:
        folder=Path(work)
        compiler=[os.environ.get("CXX","c++"),"-std=c++11","-O2","-Wall","-Wextra","-Werror","-fsanitize=undefined","-fno-sanitize-recover=all"]
        for name,files in (("element",["element_stock.cpp","tests/test_element.cpp"]),("duck",["test_duck.cpp"]),("value-random",["test_value_random.cpp"])):
            module=tests/name
            binary=folder/name
            subprocess.run([*compiler,"-I"+str(module/"tests/mock"),"-I"+str(module),"-I"+str(sdk/"sdk/include"),
                            *(str(module/f) for f in files),"-o",str(binary)],check=True)
            subprocess.run([binary],check=True)
        for module,check,params in (("element","tests/check_stock.py",[stock]),("duck","test_stock.py",[sdk,stock]),
                                    ("value-random","verify_stock_table.py",["--sdk",sdk,"--stock",stock,"--test",folder/"value-random"]),
                                    ("runtime","run.py",["--sdk",sdk]),
                                    ("stock-ui","check_stock_controls.py",["--sdk",sdk,"--stock",stock]),
                                    ("stock-ui","test_duck_ui.py",["--sdk",sdk,"--stock",stock]),
                                    ("stock-ui","check_labels.py",["--sdk",sdk]),
                                    ("stock-ui","check_screen_bindings.py",["--sdk",sdk,"--stock",stock])):
            subprocess.run([sys.executable,"-B",tests/module/check,*params],check=True)

if __name__=="__main__":
    main()
