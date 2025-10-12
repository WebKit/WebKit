"""
Test case registry for WebAssembly debugger tests
"""

from typing import List, Dict, Type
from .base import BaseTestCase


class TestRegistry:
    """Registry for managing test cases"""

    def __init__(self):
        self.test_classes: Dict[str, Type[BaseTestCase]] = {}
        self.test_order: List[str] = []

    def register_test(self, test_class: Type[BaseTestCase], name: str = None):
        """
        Register a test case class

        Args:
            test_class: The test case class to register
            name: Optional name override (defaults to class name)
        """
        test_name = name or test_class.__name__
        self.test_classes[test_name] = test_class
        if test_name not in self.test_order:
            self.test_order.append(test_name)

    def get_test_class(self, name: str) -> Type[BaseTestCase]:
        """Get a test class by name"""
        return self.test_classes.get(name)

    def get_all_test_names(self) -> List[str]:
        """Get all registered test names in order"""
        return self.test_order.copy()

    def create_test_instance(
        self, name: str, build_config: str = None, port: int = None
    ) -> BaseTestCase:
        """Create an instance of a test case by name"""
        test_class = self.get_test_class(name)
        if not test_class:
            raise ValueError(f"Test '{name}' not found in registry")
        return test_class(build_config=build_config, port=port)
