"""
WebAssembly Debugger Test Framework
A modular test framework for WebAssembly debugging functionality with auto-discovery
"""

from .core.base import BaseTestCase, TestResult
from .core.environment import WebKitEnvironment
from .runners.process_manager import ProcessManager
from .runners.sequential import WebAssemblyDebuggerTestRunner
from .core.registry import TestRegistry
from .core.utils import Colors, Logger
from .discovery.auto_discovery import (
    TestCaseDiscovery,
    create_auto_registered_runner,
    get_all_test_classes,
    list_available_tests,
    auto_import_test_cases,
)

__all__ = [
    "BaseTestCase",
    "TestResult",
    "WebKitEnvironment",
    "ProcessManager",
    "WebAssemblyDebuggerTestRunner",
    "TestRegistry",
    "Colors",
    "Logger",
    # Auto-discovery system
    "TestCaseDiscovery",
    "create_auto_registered_runner",
    "get_all_test_classes",
    "list_available_tests",
    "auto_import_test_cases",
]
