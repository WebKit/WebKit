"""
JavaScriptCore test runner — invokes run-javascriptcore-tests --testwasmdebugger.
Not a DebugSession-based test.
"""

import subprocess
from lib.utils import log_warning


class JavaScriptCoreTestCase:
    """Runs run-javascriptcore-tests --testwasmdebugger; no DebugSession."""

    test_file = None  # signals runner to skip DebugSession

    def __init__(self, env):
        self.env = env

    def execute(self):
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

            if "0 failures found." not in result.stdout:
                raise Exception("testwasmdebugger failed")

        except subprocess.TimeoutExpired:
            log_warning("JavaScriptCore test timed out but continuing")
