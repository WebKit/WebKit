"""
Automatic test case discovery and registration for WebAssembly debugger tests
"""

import os
import importlib
import inspect
from typing import List, Dict, Type, Set
from pathlib import Path

from ..core.base import BaseTestCase
from ..core.registry import TestRegistry
from ..core.utils import Logger


class TestCaseDiscovery:
    """Automatically discovers and registers test cases"""

    def __init__(self, test_cases_dir: str = None):
        self.logger = Logger()
        if test_cases_dir is None:
            # Default to the tests directory relative to this file
            current_dir = Path(__file__).parent.parent.parent
            self.test_cases_dir = current_dir / "tests"
        else:
            self.test_cases_dir = Path(test_cases_dir)

    def discover_test_cases(self) -> Dict[str, Type[BaseTestCase]]:
        """
        Discover all test case classes in the test_cases directory

        Returns:
            Dictionary mapping test case names to their classes
        """
        discovered_tests = {}

        if not self.test_cases_dir.exists():
            self.logger.warning(
                f"Test cases directory not found: {self.test_cases_dir}"
            )
            return discovered_tests

        # Get all Python files in the test_cases directory
        python_files = list(self.test_cases_dir.glob("*.py"))

        for py_file in python_files:
            if py_file.name.startswith("__"):
                continue  # Skip __init__.py and __pycache__

            try:
                # Import the module
                module_name = f"tests.{py_file.stem}"
                module = importlib.import_module(module_name)

                # Find all test case classes in the module
                test_classes = self._extract_test_classes(module)
                discovered_tests.update(test_classes)

            except Exception as e:
                self.logger.warning(f"Failed to import {py_file.name}: {e}")

        return discovered_tests

    def _extract_test_classes(self, module) -> Dict[str, Type[BaseTestCase]]:
        """Extract test case classes from a module"""
        test_classes = {}

        for name, obj in inspect.getmembers(module):
            if (
                inspect.isclass(obj)
                and issubclass(obj, BaseTestCase)
                and obj != BaseTestCase  # Don't include the base class itself
                and not name.startswith("_")
            ):  # Skip private classes

                test_classes[name] = obj

        return test_classes

    def auto_register_tests(self, registry: TestRegistry) -> int:
        """
        Automatically discover and register all test cases

        Args:
            registry: The test registry to register tests with

        Returns:
            Number of tests registered
        """
        discovered_tests = self.discover_test_cases()

        # Sort test names for consistent ordering
        sorted_test_names = sorted(discovered_tests.keys())

        for test_name in sorted_test_names:
            test_class = discovered_tests[test_name]
            registry.register_test(test_class, test_name)

        return len(discovered_tests)


def create_auto_registered_runner(
    build_config: str = None, verbose_wasm_debugger: bool = False
):
    """Create a test runner with automatically discovered and registered test cases"""
    from ..runners.sequential import WebAssemblyDebuggerTestRunner

    runner = WebAssemblyDebuggerTestRunner(
        build_config=build_config, verbose_wasm_debugger=verbose_wasm_debugger
    )
    discovery = TestCaseDiscovery()
    discovery.auto_register_tests(runner.registry)

    return runner


def get_all_test_classes() -> Dict[str, Type[BaseTestCase]]:
    """Get all available test case classes without registering them"""
    discovery = TestCaseDiscovery()
    return discovery.discover_test_cases()


def list_available_tests() -> List[str]:
    """List all available test case names"""
    test_classes = get_all_test_classes()
    return sorted(test_classes.keys())


def auto_import_test_cases():
    """Auto-import and return all test case classes for manual registration"""
    return get_all_test_classes()
