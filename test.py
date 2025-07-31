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
    parser.add_argument('--suite', type=str, choices=['parser', 'eval', 'all'], default='all',
                       help='Which test suite to run (default: all)')
    return parser.parse_args()

def exec_parser(filepath):
    cmd = [args.impl, filepath]
    # Get stderr as string and return that
    result = subprocess.run(cmd, capture_output=True,  text=True)
    return result.stderr.strip()

def exec_eval(filepath):
    cmd = [args.impl, filepath]
    # Get stdout as string and return that
    result = subprocess.run(cmd, capture_output=True, text=True)
    return result.stdout.strip()

def run_parser_tests():
    global fail_tests, fail_count, need_update_tests
    
    tests = glob.glob("test/parser/*.tcl")
    for f in tests:
        out_f = f.replace('.tcl', '.out')
        if not os.path.exists(out_f):
            fail_tests[f] = "no output file"
            fail_count += 1
            need_update_tests.append(f)
        else:
            if exec_parser(f) != open(out_f, 'r').read().strip():
                fail_tests[f] = "output mismatch"
                fail_count += 1
                need_update_tests.append(f)

        if args.update:
            for test in need_update_tests:
                with open(test.replace('.tcl', '.out'), 'w') as file_handle:
                    file_handle.write(exec_parser(test))
    
    return tests

def run_eval_tests():
    global fail_tests, fail_count, need_update_tests
    
    tests = glob.glob("test/eval/*.tcl")
    for f in tests:
        out_f = f.replace('.tcl', '.out')
        if not os.path.exists(out_f):
            fail_tests[f] = "no output file"
            fail_count += 1
            need_update_tests.append(f)
        else:
            if exec_eval(f) != open(out_f, 'r').read().strip():
                fail_tests[f] = "output mismatch"
                fail_count += 1
                need_update_tests.append(f)

        if args.update:
            for test in need_update_tests:
                with open(test.replace('.tcl', '.out'), 'w') as file_handle:
                    file_handle.write(exec_eval(test))
    
    return tests

if __name__ == "__main__":
    args = parse_args()
    
    all_tests = []
    
    if args.suite in ['parser', 'all']:
        all_tests.extend(run_parser_tests())
    
    if args.suite in ['eval', 'all']:
        all_tests.extend(run_eval_tests())

    for f, err in fail_tests.items():
        print(f"FAILED: {f} ({err})")
    print("---")
    if args.update:
        print("UPDATED TESTS: ", need_update_tests)
    print(f"PASSED {len(all_tests) - fail_count}/{len(all_tests)} TESTS")
    if fail_count > 0:
        sys.exit(1)


