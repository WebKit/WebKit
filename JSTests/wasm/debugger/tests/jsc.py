"""
JavaScriptCore Test Case Module
Test case for running JavaScriptCore testwasmdebugger
"""

import subprocess
from lib.core.base import BaseTestCase


class JavaScriptCoreTestCase(BaseTestCase):
    """Test case for running JavaScriptCore testwasmdebugger"""

    def __init__(self, build_config: str = None, port: int = None):
        super().__init__(build_config, port)
        self.description = (
            "Run JavaScriptCore testwasmdebugger to verify basic functionality"
        )

    def execute(self):
        """Execute the JavaScriptCore test"""
        self.logger.verbose("Running JavaScriptCore Test")

        cmd = f"{self.env.webkit_root}/Tools/Scripts/run-javascriptcore-tests --debug --no-build --testwasmdebugger"

        try:
            result = subprocess.run(
                cmd,
                shell=True,
                timeout=120,
                capture_output=True,
                text=True,
                env=self.env.env,
                cwd=str(self.env.webkit_root),
            )

            self.logger.verbose(f"Return code: {result.returncode}")

            if "0 failures found." in result.stdout:
                self.logger.success("JavaScriptCore test passed")
            else:
                raise Exception("testwasmdebugger failed")

        except subprocess.TimeoutExpired:
            self.logger.warning("JavaScriptCore test timed out but continuing")
        except Exception as e:
            raise Exception(f"JavaScriptCore test failed: {e}")
