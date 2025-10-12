"""
Parallel test runner for WebAssembly debugger tests
"""

import os
import time
import threading
import subprocess
from concurrent.futures import ThreadPoolExecutor, as_completed
from typing import List, Optional, Dict, Any

from ..core.base import TestResult
from ..core.registry import TestRegistry
from ..core.utils import Logger


class ParallelWebAssemblyDebuggerTestRunner:
    """
    Simplified parallel test runner with centralized port management

    Architecture:
    - Runner manages port allocation (no singleton needed)
    - Each test gets a unique port assigned before execution
    - Workers execute tests with pre-allocated ports
    - Clean task-worker assignment with proper isolation
    """

    def __init__(
        self,
        build_config: Optional[str] = None,
        verbose_wasm_debugger: bool = False,
        max_workers: int = 4,
        verbose_parallel: bool = False,
    ):
        self.logger = Logger()
        self.registry = TestRegistry()
        self.build_config = build_config
        self.verbose_wasm_debugger = verbose_wasm_debugger
        self.max_workers = max_workers
        self.verbose_parallel = verbose_parallel
        self.results: List[TestResult] = []
        self.script_start_time = None

        # Simple port management
        self._port_counter = 0
        self._port_lock = threading.Lock()
        self._results_lock = threading.Lock()

        # Process and thread tracking
        self._process_thread_tracker = ProcessThreadTracker()

    def _allocate_port(self) -> int:
        """Allocate a unique port for a test case"""
        with self._port_lock:
            port = 12340 + self._port_counter
            self._port_counter += 1
            return port

    def run_all_tests_parallel(self) -> List[TestResult]:
        """Run all registered tests in parallel with pre-allocated ports"""
        test_names = self.registry.get_all_test_names()
        if not test_names:
            self.logger.warning("No tests registered")
            return []
        return self._run_tests_parallel(
            test_names, "Starting Parallel WebAssembly Debugger Test Suite"
        )

    def run_specific_tests_parallel(self, test_names: List[str]) -> List[TestResult]:
        """Run specific tests by name in parallel with pre-allocated ports"""
        # Validate test names
        valid_test_names = [
            name for name in test_names if self.registry.get_test_class(name)
        ]
        invalid_tests = [name for name in test_names if name not in valid_test_names]

        for invalid_test in invalid_tests:
            self.logger.error(f"Test '{invalid_test}' not found in registry")

        if not valid_test_names:
            self.logger.warning("No valid tests to run")
            return []

        header_msg = f"Running {len(valid_test_names)} specific tests in parallel"
        return self._run_tests_parallel(valid_test_names, header_msg)

    def _run_tests_parallel(
        self, test_names: List[str], header_msg: str
    ) -> List[TestResult]:
        """Simple parallel execution with process/thread tracking"""
        self.script_start_time = time.time()
        self.results.clear()

        if not self.verbose_parallel:
            self.logger.set_quiet_parallel(True)

        # Track main framework process
        self._process_thread_tracker.track_main_process()

        self.logger.header(f"🚀 {header_msg} (max_workers={self.max_workers})")
        self.logger.info(f"Found {len(test_names)} tests to run in parallel")

        # Pre-allocate ports
        port_assignments = {}
        for test_name in test_names:
            port_assignments[test_name] = self._allocate_port()
            self.logger.verbose(
                f"📋 Pre-allocated port {port_assignments[test_name]} for test {test_name}"
            )

        # Run tests in parallel
        with ThreadPoolExecutor(
            max_workers=min(self.max_workers, len(test_names))
        ) as executor:
            future_to_test = {
                executor.submit(
                    self._execute_single_test, name, port_assignments[name]
                ): (name, port_assignments[name])
                for name in test_names
            }

            for future in as_completed(future_to_test):
                test_name, port = future_to_test[future]
                try:
                    result = future.result()
                    with self._results_lock:
                        self.results.append(result)

                    status = "✅ PASSED" if result.success else "❌ FAILED"
                    self.logger.info(
                        f"{status} {test_name} (Port: {port}, Time: {result.duration():.2f}s)"
                    )

                except Exception as e:
                    result = TestResult(test_name)
                    result.mark_failure(f"Parallel execution error: {str(e)}")
                    with self._results_lock:
                        self.results.append(result)
                    self.logger.error(f"❌ FAILED {test_name} (Port: {port}): {str(e)}")

        self.results.sort(key=lambda r: r.name)
        self._print_summary()

        # Only show process/thread tracking in verbose mode
        if self.verbose_parallel:
            self._print_process_thread_summary()

        return self.results

    def _execute_single_test(self, test_name: str, port: int) -> TestResult:
        """
        Execute a single test case with pre-allocated port and track processes/threads

        Args:
            test_name: Name of the test to execute
            port: Pre-allocated port for this test

        Returns:
            TestResult for the executed test
        """
        try:
            # Track worker thread
            worker_tid = threading.get_ident()
            self._process_thread_tracker.track_worker_thread(worker_tid, test_name)

            self.logger.parallel_step(f"Setting up test: {test_name}")

            # Create test instance with pre-allocated port
            test_instance = self.registry.create_test_instance(
                test_name, build_config=self.build_config, port=port
            )

            # Inject tracker into test instance
            if hasattr(test_instance, "set_process_tracker"):
                test_instance.set_process_tracker(
                    self._process_thread_tracker, worker_tid
                )

            # Set the verbose wasm debugger flag on the test instance
            if hasattr(test_instance, "set_verbose_wasm_debugger"):
                test_instance.set_verbose_wasm_debugger(self.verbose_wasm_debugger)

            self.logger.parallel_step(f"Setting up debugging session for {test_name}")

            # Run the test (each test has its own ProcessManager with unique port)
            result = test_instance.run()

            status = "passed" if result.success else "failed"
            self.logger.parallel_step(
                f"Test {test_name} {status} ({result.duration():.2f}s)"
            )

            self.logger.parallel_step(f"Tearing down test: {test_name}")

            return result

        except Exception as e:
            # Create failed result for exceptions during test creation/execution
            result = TestResult(test_name)
            result.mark_failure(f"Parallel test execution error: {str(e)}")
            self.logger.error(f"❌ Test {test_name} failed on port {port}: {str(e)}")
            return result

    def _print_summary(self):
        """Print simple test run summary"""
        if not self.results:
            return

        total_tests = len(self.results)
        passed_tests = sum(1 for r in self.results if r.success)
        failed_tests = total_tests - passed_tests
        test_execution_time = sum(r.duration() for r in self.results)
        script_runtime = (
            time.time() - self.script_start_time if self.script_start_time else 0
        )

        self.logger.header("📊 Parallel Test Summary")
        self.logger.info(f"Total tests: {total_tests}")
        self.logger.info(f"Passed: {passed_tests}")
        self.logger.info(f"Failed: {failed_tests}")
        self.logger.info(f"Total test execution time: {test_execution_time:.2f}s")
        self.logger.info(f"Wall clock time: {script_runtime:.2f}s")

        if script_runtime > 0 and test_execution_time > 0:
            speedup = test_execution_time / script_runtime
            efficiency = (speedup / min(self.max_workers, total_tests)) * 100
            self.logger.info(f"Parallelization speedup: {speedup:.2f}x")
            self.logger.info(f"Parallel efficiency: {efficiency:.1f}%")

        if failed_tests > 0:
            self.logger.subheader("❌ Failed Tests:")
            for result in self.results:
                if not result.success:
                    self.logger.error(f"  • {result.name}: {result.error_message}")

        if failed_tests == 0:
            self.logger.success("🎉 All parallel tests passed!")
        else:
            self.logger.error(f"💥 {failed_tests} parallel test(s) failed")

    def _print_process_thread_summary(self):
        """Print detailed process and thread tracking summary"""
        self.logger.header("🔍 Process & Thread Tracking Summary")
        self._process_thread_tracker.print_summary()

    def get_failed_count(self) -> int:
        """Get number of failed tests"""
        with self._results_lock:
            return sum(1 for r in self.results if not r.success)

    def cleanup(self):
        """Simple cleanup of resources"""
        self.logger.info("🧹 Cleaning up parallel test runner resources")

        try:
            subprocess.run(
                "pkill -f 'jsc.*--wasm-debug'",
                shell=True,
                capture_output=True,
                timeout=5,
            )
        except (subprocess.TimeoutExpired, Exception):
            pass

        with self._port_lock:
            self._port_counter = 0

        self.logger.success("✅ Parallel test runner cleanup complete")


class ProcessThreadTracker:
    """Track processes and threads created during parallel test execution"""

    def __init__(self):
        self.main_pid = None
        self.main_tid = None
        self.worker_threads = (
            {}
        )  # tid -> {test_cases: [], current_test: str, processes: {}, monitor_threads: []}
        self.lock = threading.Lock()

    def track_main_process(self):
        """Track the main framework process"""
        self.main_pid = os.getpid()
        self.main_tid = threading.get_ident()

    def track_worker_thread(self, worker_tid: int, test_name: str):
        """Track a worker thread and the test case it's working on"""
        with self.lock:
            if worker_tid not in self.worker_threads:
                self.worker_threads[worker_tid] = {
                    "test_cases": [],
                    "current_test": test_name,
                    "processes": {},
                    "monitor_threads": [],
                }
            else:
                # Worker is handling a new test case
                self.worker_threads[worker_tid]["current_test"] = test_name

            # Add this test case to the list if not already there
            if test_name not in self.worker_threads[worker_tid]["test_cases"]:
                self.worker_threads[worker_tid]["test_cases"].append(test_name)

    def track_process(self, worker_tid: int, process_type: str, pid: int):
        """Track a process created by a worker thread for current test"""
        with self.lock:
            if worker_tid in self.worker_threads:
                current_test = self.worker_threads[worker_tid]["current_test"]
                key = f"{current_test}_{process_type}"
                self.worker_threads[worker_tid]["processes"][key] = pid

    def track_monitor_thread(
        self, worker_tid: int, monitor_tid: int, process_type: str, stream_type: str
    ):
        """Track a monitor thread created by a worker thread for current test"""
        with self.lock:
            if worker_tid in self.worker_threads:
                current_test = self.worker_threads[worker_tid]["current_test"]
                self.worker_threads[worker_tid]["monitor_threads"].append(
                    {
                        "tid": monitor_tid,
                        "process_type": process_type,
                        "stream_type": stream_type,
                        "test_case": current_test,
                    }
                )

    def print_summary(self):
        """Print detailed process and thread summary"""
        logger = Logger()

        # Main framework process
        logger.info(f"Framework Main Process:")
        logger.info(f"  PID: {self.main_pid} TID: {self.main_tid} (main thread)")

        # Worker threads and their resources
        worker_count = 0
        total_processes = 0
        total_monitor_threads = 0

        for worker_tid, data in self.worker_threads.items():
            worker_count += 1
            test_cases = data["test_cases"]
            processes = data["processes"]
            monitor_threads = data["monitor_threads"]

            logger.info(
                f"  PID: {self.main_pid} TID: {worker_tid} (worker thread {worker_count})"
            )
            logger.info(f"    Worked on test cases: {', '.join(test_cases)}")

            # Group processes and monitor threads by test case
            test_case_resources = {}
            for key, pid in processes.items():
                if "_" in key:
                    test_case, process_type = key.rsplit("_", 1)
                    if test_case not in test_case_resources:
                        test_case_resources[test_case] = {
                            "processes": {},
                            "monitor_threads": [],
                        }
                    test_case_resources[test_case]["processes"][process_type] = pid
                    total_processes += 1

            # Group monitor threads by test case
            for monitor in monitor_threads:
                test_case = monitor["test_case"]
                if test_case not in test_case_resources:
                    test_case_resources[test_case] = {
                        "processes": {},
                        "monitor_threads": [],
                    }
                test_case_resources[test_case]["monitor_threads"].append(monitor)
                total_monitor_threads += 1

            # Show resources for each test case
            for test_case in test_cases:
                if test_case in test_case_resources:
                    resources = test_case_resources[test_case]
                    logger.info(f"    Test Case: {test_case}")

                    # Show processes for this test case
                    for process_type, pid in resources["processes"].items():
                        logger.info(f"      PID: {pid} ({process_type} process)")

                    # Show monitor threads for this test case
                    if resources["monitor_threads"]:
                        logger.info(f"      Monitor Threads:")
                        for monitor in resources["monitor_threads"]:
                            logger.info(
                                f"        PID: {self.main_pid} TID: {monitor['tid']} ({monitor['process_type']} {monitor['stream_type']})"
                            )

        # Summary totals
        logger.info(f"")
        logger.info(f"📊 Resource Summary:")
        logger.info(f"  Framework Process: 1 (PID: {self.main_pid})")
        logger.info(f"  Test Processes: {total_processes}")
        logger.info(f"  Total Processes: {1 + total_processes}")
        logger.info(
            f"  Framework Threads: {1 + worker_count} (1 main + {worker_count} workers)"
        )
        logger.info(f"  Monitor Threads: {total_monitor_threads}")
        logger.info(f"  Total Threads: {1 + worker_count + total_monitor_threads}")
