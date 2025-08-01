# test.py 

# simple test suite for picol and ports of it
# snapshot tests the parser and interpreter

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
    parser.add_argument('--impl', type=str, default='./picol/picol',
                       help='Path to the implementation to test')
    parser.add_argument('--suite', type=str, choices=['parser', 'eval', 'all'], default='all',
                       help='Which test suite to run (default: all)')
    return parser.parse_args()

def exec_test(filepath):
    cmd = [args.impl, filepath]
    env = os.environ.copy()
    env['PARSER_STDERR'] = '1'
    result = subprocess.run(cmd, capture_output=True, text=True, env=env)
    return result.stdout.strip(), result.stderr.strip()

def check_output(expected_file, actual_output):
    if not os.path.exists(expected_file):
        return False, "no output file"
    
    with open(expected_file, 'r') as f:
        expected = f.read().strip()
    
    if expected != actual_output:
        return False, "output mismatch"
    
    return True, None

def update_output_file(filepath, content):
    with open(filepath, 'w') as f:
        f.write(content)
        if content and not content.endswith('\n'):
            f.write('\n')

def run_tests(suite):
    global fail_tests, fail_count, need_update_tests
    
    # All tests are now in test/ directory
    tests = glob.glob("test/*.tcl")
    tests.sort()  # Ensure consistent ordering
    
    tested = []
    for f in tests:
        # Run the test and capture both stdout and stderr
        stdout, stderr = exec_test(f)
        
        stdout_file = f.replace('.tcl', '.stdout')
        stderr_file = f.replace('.tcl', '.stderr')
        
        test_failed = False
        
        # Check based on suite type
        if suite == 'parser':
            # Parser tests primarily check stderr (token output)
            if args.update:
                update_output_file(stderr_file, stderr)
                # Create empty stdout file if it doesn't exist
                if not os.path.exists(stdout_file):
                    update_output_file(stdout_file, "")
            else:
                success, error = check_output(stderr_file, stderr)
                if not success:
                    fail_tests[f] = f"stderr {error}"
                    fail_count += 1
                    test_failed = True
                    need_update_tests.append(f)
            tested.append(f)
            
        elif suite == 'eval':
            # Eval tests primarily check stdout (evaluation output)
            if args.update:
                update_output_file(stdout_file, stdout)
                # Optionally create stderr file for token output
                if stderr or not os.path.exists(stderr_file):
                    update_output_file(stderr_file, stderr)
            else:
                success, error = check_output(stdout_file, stdout)
                if not success:
                    fail_tests[f] = f"stdout {error}"
                    fail_count += 1
                    test_failed = True
                    need_update_tests.append(f)
            tested.append(f)
            
        elif suite == 'all':
            # When running all tests, check both outputs if files exist
            if args.update:
                update_output_file(stdout_file, stdout)
                update_output_file(stderr_file, stderr)
            else:
                # Check stdout if file exists
                if os.path.exists(stdout_file):
                    success, error = check_output(stdout_file, stdout)
                    if not success:
                        fail_tests[f] = f"stdout {error}"
                        fail_count += 1
                        test_failed = True
                        need_update_tests.append(f)
                
                # Check stderr if file exists
                if os.path.exists(stderr_file):
                    success, error = check_output(stderr_file, stderr)
                    if not success:
                        fail_tests[f] = f"stderr {error}"
                        fail_count += 1
                        test_failed = True
                        if f not in need_update_tests:
                            need_update_tests.append(f)
                
                # If neither output file exists, it's a failure
                if not os.path.exists(stdout_file) and not os.path.exists(stderr_file):
                    fail_tests[f] = "no output files"
                    fail_count += 1
                    test_failed = True
                    need_update_tests.append(f)
            
            tested.append(f)
    
    return tested

if __name__ == "__main__":
    args = parse_args()
    
    all_tests = run_tests(args.suite)
    
    # Remove duplicates from need_update_tests
    need_update_tests = list(dict.fromkeys(need_update_tests))
    
    for f, err in fail_tests.items():
        print(f"FAILED: {f} ({err})")
    print("---")
    if args.update:
        print("UPDATED TESTS: ", need_update_tests)
    print(f"PASSED {len(all_tests) - fail_count}/{len(all_tests)} TESTS")
    if fail_count > 0:
        sys.exit(1)


