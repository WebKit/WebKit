#!/usr/bin/env python3
"""
WebAssembly Debugger Test Runner
"""

import argparse
import os
import sys
import time
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

from lib.session import DebugSession
from lib.environment import WebKitEnvironment
from lib.utils import log_pass, log_fail, log_info, log_header, set_verbose
from tests.tests import ALL_TESTS
from tests.jsc import JavaScriptCoreTestCase


def get_optimal_worker_count():
    cpu = os.cpu_count() or 4
    if cpu <= 2:
        return cpu
    elif cpu <= 4:
        return cpu + 1
    elif cpu <= 8:
        return cpu + 2
    else:
        return min(cpu + 4, 16)


def calculate_smart_workers(test_count, requested=None):
    if requested is not None:
        return requested
    return min(test_count, get_optimal_worker_count())


def run_one(cls, port, env, verbose, verbose_wasm_debugger):
    """Run a single test class. Returns True on pass, False on fail."""
    start = time.monotonic()

    # JavaScriptCoreTestCase runs a subprocess directly — no DebugSession.
    if getattr(cls, "test_file", None) is None:
        try:
            cls(env).execute()
            log_pass(cls.__name__, time.monotonic() - start)
            return True
        except Exception as e:
            log_fail(cls.__name__, time.monotonic() - start, e)
            return False

    extra = list(getattr(cls, "extra_jsc_options", None) or [])
    if verbose_wasm_debugger:
        extra.append("--verboseWasmDebugger=1")

    try:
        with DebugSession(
            cls.test_file, port,
            str(env.jsc_path), env.lldb_path, env.env,
            extra, verbose, cls.__name__,
        ) as session:
            instance = cls()
            instance.session = session
            instance.execute()
        log_pass(cls.__name__, time.monotonic() - start)
        return True
    except Exception as e:
        log_fail(cls.__name__, time.monotonic() - start, e)
        return False


def main():
    parser = argparse.ArgumentParser(description="WebAssembly Debugger Test Runner")
    parser.add_argument("--verbose", "-v", action="store_true",
                        help="Print all LLDB/JSC output")
    parser.add_argument("--verbose-wasm-debugger", action="store_true",
                        help="Pass --verboseWasmDebugger=1 to JSC")
    parser.add_argument("--pid-tid", action="store_true",
                        help="Prefix output with [TestName] (implied by --verbose)")
    parser.add_argument("--test", "-t", action="append", metavar="NAME",
                        help="Run specific test(s); may be repeated")
    parser.add_argument("--list", "-l", action="store_true",
                        help="List available test cases and exit")
    parser.add_argument("--parallel", "-p", type=int, nargs="?", const=-1, metavar="N",
                        help="Parallel workers (omit N for auto-detect)")

    build_group = parser.add_mutually_exclusive_group()
    build_group.add_argument("--debug",   action="store_true", help="Use Debug build")
    build_group.add_argument("--release", action="store_true", help="Use Release build")

    args = parser.parse_args()

    # --pid-tid and --verbose-wasm-debugger both imply verbose output
    verbose = args.verbose or args.pid_tid or args.verbose_wasm_debugger
    set_verbose(verbose)

    build_config = "Debug" if args.debug else "Release" if args.release else None
    env = WebKitEnvironment(Path(__file__), build_config)

    all_tests = list(ALL_TESTS) + [JavaScriptCoreTestCase]

    if args.list:
        for cls in all_tests:
            print(cls.__name__)
        return

    # Filter by --test names if given
    if args.test:
        requested = set(args.test)
        tests = [cls for cls in all_tests if cls.__name__ in requested]
        missing = requested - {cls.__name__ for cls in tests}
        for name in sorted(missing):
            log_info(f"Unknown test: {name}")
        if not tests:
            log_info("No matching tests found.")
            sys.exit(1)
    else:
        tests = all_tests

    # Assign stable ports: index in all_tests (not filtered list) so ports
    # don't shift when --test filters a subset.
    port_map = {cls: 12340 + i for i, cls in enumerate(all_tests)}
    tasks = [(cls, port_map[cls], env, verbose, args.verbose_wasm_debugger)
             for cls in tests]

    parallel = args.parallel is not None
    requested_workers = None if args.parallel == -1 else args.parallel

    log_header(f"Running {len(tests)} test(s)" +
               (f" — parallel={calculate_smart_workers(len(tests), requested_workers)}"
                if parallel else " — sequential"))

    if parallel:
        workers = calculate_smart_workers(len(tests), requested_workers)
        with ThreadPoolExecutor(max_workers=workers) as pool:
            results = list(pool.map(lambda a: run_one(*a), tasks))
    else:
        results = [run_one(*a) for a in tasks]

    passed = sum(results)
    failed = len(results) - passed
    log_header(f"{passed}/{len(results)} passed" + (f", {failed} failed" if failed else ""))
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
