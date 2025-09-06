"""
WebAssembly Debugger Test Framework
A modular test framework for WebAssembly debugging functionality with auto-discovery
"""

from .base import BaseTestCase, TestResult
from .environment import WebKitEnvironment
from .process_manager import ProcessManager
from .runner import WebAssemblyDebuggerTestRunner
from .registry import TestRegistry
from .utils import Colors, Logger
from .auto_discovery import (
    TestCaseDiscovery,
    create_auto_registered_runner,
    get_all_test_classes,
    list_available_tests,
    auto_import_test_cases
)

__all__ = [
    'BaseTestCase',
    'TestResult',
    'WebKitEnvironment',
    'ProcessManager',
    'WebAssemblyDebuggerTestRunner',
    'TestRegistry',
    'Colors',
    'Logger',
    # Auto-discovery system
    'TestCaseDiscovery',
    'create_auto_registered_runner',
    'get_all_test_classes',
    'list_available_tests',
    'auto_import_test_cases'
]