"""
Sequential test runner for WebAssembly debugger tests
"""

import time
from typing import List, Optional

from ..core.base import BaseTestCase, TestResult
from ..core.registry import TestRegistry
from ..core.utils import Logger


class WebAssemblyDebuggerTestRunner:
    """Test runner for WebAssembly debugger tests"""

    def __init__(
        self, build_config: Optional[str] = None, verbose_wasm_debugger: bool = False
    ):
        self.logger = Logger()
        self.registry = TestRegistry()
        self.results: List[TestResult] = []
        self.build_config = build_config
        self.verbose_wasm_debugger = verbose_wasm_debugger
        self.script_start_time = None

        # Sequential port allocation (simple counter)
        self._current_port = 12340

    def run_all_tests(self) -> List[TestResult]:
        """Run all registered tests"""
        self.script_start_time = time.time()
        self.logger.header("🚀 Starting WebAssembly Debugger Test Suite")

        test_names = self.registry.get_all_test_names()
        if not test_names:
            self.logger.warning("No tests registered")
            return []

        self.logger.info(f"Found {len(test_names)} tests to run")

        # Clear previous results
        self.results.clear()

        # Run each test
        for test_name in test_names:
            self.logger.subheader(f"Running test: {test_name}")
            result = self._run_single_test(test_name)
            self.results.append(result)

            # Result already logged by test.run(), no need for duplicate logging

        # Print summary
        self._print_summary()

        return self.results

    def run_specific_tests(self, test_names: List[str]) -> List[TestResult]:
        """Run specific tests by name"""
        self.script_start_time = time.time()
        self.logger.header(f"🚀 Running {len(test_names)} specific tests")

        # Clear previous results
        self.results.clear()

        # Run each specified test
        for test_name in test_names:
            if not self.registry.get_test_class(test_name):
                self.logger.error(f"Test '{test_name}' not found in registry")
                continue

            self.logger.subheader(f"Running test: {test_name}")
            result = self._run_single_test(test_name)
            self.results.append(result)

            # Result already logged by test.run(), no need for duplicate logging

        # Print summary
        self._print_summary()

        return self.results

    def _run_single_test(self, test_name: str) -> TestResult:
        """Run a single test case with sequential port allocation"""
        try:
            # Allocate port for sequential execution
            port = self._current_port
            self._current_port += 1

            # Create test instance with build configuration, port, and verbose wasm debugger flag
            test_instance = self.registry.create_test_instance(
                test_name, self.build_config, port=port
            )

            # Set the verbose wasm debugger flag on the test instance
            if hasattr(test_instance, "set_verbose_wasm_debugger"):
                test_instance.set_verbose_wasm_debugger(self.verbose_wasm_debugger)

            # Run the test
            result = test_instance.run()

            return result

        except Exception as e:
            # Create failed result for exceptions during test creation/execution
            result = TestResult(test_name)
            result.mark_failure(f"Test execution error: {str(e)}")
            return result

    def _print_summary(self):
        """Print test run summary"""
        if not self.results:
            return

        total_tests = len(self.results)
        passed_tests = sum(1 for r in self.results if r.success)
        failed_tests = total_tests - passed_tests
        test_execution_time = sum(r.duration() for r in self.results)

        # Calculate total script runtime
        script_runtime = (
            time.time() - self.script_start_time if self.script_start_time else 0
        )

        self.logger.header("📊 Test Summary")
        self.logger.info(f"Total tests: {total_tests}")
        self.logger.info(f"Passed: {passed_tests}")
        self.logger.info(f"Failed: {failed_tests}")
        self.logger.info(f"Test execution time: {test_execution_time:.2f}s")
        self.logger.info(f"Total script runtime: {script_runtime:.2f}s")

        if failed_tests > 0:
            self.logger.subheader("❌ Failed Tests:")
            for result in self.results:
                if not result.success:
                    self.logger.error(f"  • {result.name}: {result.error_message}")

        # Overall result
        if failed_tests == 0:
            self.logger.success("🎉 All tests passed!")
        else:
            self.logger.error(f"💥 {failed_tests} test(s) failed")

    def get_failed_count(self) -> int:
        """Get number of failed tests"""
        return sum(1 for r in self.results if not r.success)

    def cleanup(self):
        """Cleanup any remaining resources"""
        self.logger.info("🧹 Cleaning up test runner resources")

        import subprocess

        try:
            subprocess.run(
                "pkill -f 'jsc.*--wasm-debug'", shell=True, capture_output=True
            )
        except Exception:
            pass

        self.logger.success("✅ Test runner cleanup complete")
