# test.py 
# simple test suite for picol and ports of it
# snapshot tests the parser

import argparse
import glob
import subprocess
import os
import sys

fail_tests = {}
fail_count = 0
need_update_tests = []

def parse_args():
    parser = argparse.ArgumentParser(description='Test suite for picol and its ports')
    parser.add_argument('--update', action='store_true', 
                       help='Update test output files instead of checking them')
    parser.add_argument('--impl', type=str, default='./picol/picol-parser',
                       help='Path to the implementation to test')
    return parser.parse_args()

def exec_parser(filepath):
    cmd = [args.impl, filepath]
    # Get stderr as string and return that
    result = subprocess.run(cmd, capture_output=True,  text=True)
    return result.stderr.strip()

if __name__ == "__main__":
    args = parse_args()
    update = args.update

    tests = glob.glob("test/parser/*.tcl")
    for f in glob.glob(f"test/parser/*.tcl"):
        out_f = f.replace('.tcl', '.out')
        if not os.path.exists(out_f):
            fail_tests[f] = "no output file"
            fail_count += 1
            need_update_tests.append(f)
        else:
            if exec_parser(f) != open(out_f, 'r').read():
                fail_tests[f] = "output mismatch"
                fail_count += 1
                need_update_tests.append(f)

        if update:
            for test in need_update_tests:
                with open(test.replace('.tcl', '.out'), 'w') as file_handle:
                    file_handle.write(exec_parser(test))

    for f, err in fail_tests.items():
        print(f"FAILED: {f} ({err})")
    print("---")
    if update:
        print("UPDATED TESTS: ", need_update_tests)
    print(f"PASSED {len(tests) - fail_count}/{len(tests)} TESTS")
    if fail_count > 0:
        sys.exit(1)


