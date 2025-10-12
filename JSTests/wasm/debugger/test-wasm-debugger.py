#!/usr/bin/env python3
"""
WebAssembly Debugger Test Framework
"""

import sys
import os
import argparse
from lib.core.utils import Logger
from lib.runners.parallel import ParallelWebAssemblyDebuggerTestRunner
from lib.discovery.auto_discovery import (
    create_auto_registered_runner,
    TestCaseDiscovery,
)

# Prevent creation of __pycache__ directories
sys.dont_write_bytecode = True


def get_optimal_worker_count():
    """Calculate optimal number of workers based on system capabilities"""
    try:
        # Get CPU count
        cpu_count = os.cpu_count() or 4

        # For I/O-bound WebAssembly debugging tests, we can use more workers than CPU cores
        # But not too many to avoid resource contention
        if cpu_count <= 2:
            return cpu_count
        elif cpu_count <= 4:
            return cpu_count + 1  # 3-5 workers
        elif cpu_count <= 8:
            return cpu_count + 2  # 6-10 workers
        else:
            return min(cpu_count + 4, 16)  # Cap at 16 workers max
    except Exception:
        return 4  # Safe fallback


def calculate_smart_workers(test_count, requested_workers=None):
    """Calculate smart worker count based on tests and system"""
    if requested_workers is not None:
        return requested_workers

    optimal_workers = get_optimal_worker_count()
    # Don't use more workers than tests (no point in having idle workers)
    return min(test_count, optimal_workers)


def create_test_runner(
    verbose=False, build_config=None, parallel=False, workers=None, test_names=None
):
    """Create test runner with auto-discovered test cases and smart worker calculation"""
    if parallel:
        # First create a temporary runner to get test count
        temp_runner = create_auto_registered_runner(
            build_config=build_config, verbose_wasm_debugger=False
        )

        # Calculate test count
        if test_names:
            test_count = len(
                [
                    name
                    for name in test_names
                    if temp_runner.registry.get_test_class(name)
                ]
            )
        else:
            test_count = len(temp_runner.registry.get_all_test_names())

        # Calculate smart worker count
        smart_workers = calculate_smart_workers(test_count, workers)

        if verbose:
            Logger.info(
                f"Smart parallel execution: {test_count} tests, {smart_workers} workers (CPU cores: {os.cpu_count()})"
            )

        runner = ParallelWebAssemblyDebuggerTestRunner(
            build_config=build_config,
            verbose_wasm_debugger=False,
            max_workers=smart_workers,
            verbose_parallel=verbose,
        )
        discovery = TestCaseDiscovery()
        discovery.auto_register_tests(runner.registry)
    else:
        runner = create_auto_registered_runner(
            build_config=build_config, verbose_wasm_debugger=False
        )
    return runner


def main():
    """Main entry point"""
    parser = argparse.ArgumentParser(description="WebAssembly Debugger Test Framework")
    parser.add_argument(
        "--verbose", "-v", action="store_true", help="Enable verbose logging"
    )
    parser.add_argument(
        "--pid-tid", action="store_true", help="Enable PID/TID prefixes"
    )
    parser.add_argument(
        "--test", "-t", action="append", help="Run specific test case(s)"
    )
    parser.add_argument(
        "--list", "-l", action="store_true", help="List available test cases"
    )
    parser.add_argument(
        "--parallel",
        "-p",
        type=int,
        nargs="?",
        const=-1,
        metavar="N",
        help="Enable parallel execution with N workers (default: auto-detect based on tests and CPU)",
    )

    build_group = parser.add_mutually_exclusive_group()
    build_group.add_argument("--debug", action="store_true", help="Use Debug build")
    build_group.add_argument("--release", action="store_true", help="Use Release build")

    args = parser.parse_args()

    build_config = "Debug" if args.debug else "Release" if args.release else None
    parallel = args.parallel is not None
    # Use -1 as sentinel for auto-detect, otherwise use specified value
    workers = None if args.parallel == -1 else args.parallel

    if args.verbose:
        Logger.set_verbose(True)

    if args.pid_tid or parallel:
        Logger.set_pid_tid_logging(True)

    runner = create_test_runner(
        args.verbose, build_config, parallel, workers, args.test
    )

    if args.list:
        print("Available test cases:")
        for name in runner.registry.get_all_test_names():
            print(f"  {name}")
        return

    try:
        if args.test:
            results = (
                runner.run_specific_tests_parallel(args.test)
                if parallel
                else runner.run_specific_tests(args.test)
            )
        else:
            results = (
                runner.run_all_tests_parallel() if parallel else runner.run_all_tests()
            )

        sys.exit(1 if runner.get_failed_count() > 0 else 0)

    except KeyboardInterrupt:
        Logger.info("Test interrupted by user")
        sys.exit(1)
    except Exception as e:
        Logger.error(f"Test suite failed: {e}")
        sys.exit(1)
    finally:
        runner.cleanup()
        Logger.cleanup_interception()


if __name__ == "__main__":
    main()
