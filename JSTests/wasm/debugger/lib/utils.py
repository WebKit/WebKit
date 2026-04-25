"""
Logging utilities for the WebAssembly debugger test framework.
"""

import sys

_verbose = False


def set_verbose(enabled: bool):
    global _verbose
    _verbose = enabled


class Colors:
    GREEN = "\033[92m"
    RED = "\033[91m"
    YELLOW = "\033[93m"
    BLUE = "\033[94m"
    CYAN = "\033[96m"
    RESET = "\033[0m"
    BOLD = "\033[1m"
    DIM = "\033[2m"


def log_pass(name, duration):
    print(f"{Colors.GREEN}PASS  {name} ({duration:.1f}s){Colors.RESET}")


def log_fail(name, duration, err):
    print(f"{Colors.RED}FAIL  {name} ({duration:.1f}s): {err}{Colors.RESET}", file=sys.stderr)


def log_info(msg):
    print(f"{Colors.BLUE}{msg}{Colors.RESET}")


def log_warning(msg):
    print(f"{Colors.YELLOW}WARNING: {msg}{Colors.RESET}")


def log_error(msg):
    print(f"{Colors.RED}ERROR: {msg}{Colors.RESET}", file=sys.stderr)


def log_success(msg):
    print(f"{Colors.GREEN}{msg}{Colors.RESET}")


def log_verbose(msg):
    if _verbose:
        print(f"{Colors.DIM}{msg}{Colors.RESET}")


def log_header(msg):
    print(f"\n{Colors.BOLD}{Colors.CYAN}=== {msg} ==={Colors.RESET}")
