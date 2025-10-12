"""
Process management for WebAssembly debugger tests
"""

import subprocess
import threading
import time
from typing import Optional, Dict, Any

from ..core.utils import Logger


class ProcessManager:
    """
    Per-test process manager for isolated WebAssembly debugger test execution.
    Each test case gets its own ProcessManager instance with a pre-allocated port.
    """

    def __init__(self, port: int):
        self.logger = Logger()
        self.port = port

        # Process tracking (isolated per test)
        self.debugger_process: Optional[subprocess.Popen] = None
        self.lldb_process: Optional[subprocess.Popen] = None
        self.processes: Dict[str, subprocess.Popen] = {}

        # Process management
        self._cleanup_lock = threading.Lock()
        self._is_cleaned_up = False

    def register_processes(
        self,
        debugger_process: subprocess.Popen,
        lldb_process: subprocess.Popen,
        test_name: str,
    ):
        """Register processes for tracking and cleanup"""
        with self._cleanup_lock:
            self.debugger_process = debugger_process
            self.lldb_process = lldb_process
            self.processes = {"debugger": debugger_process, "lldb": lldb_process}

        self.logger.info(f"🔧 Registered processes for {test_name} on port {self.port}")

    def cleanup_processes(self):
        """Clean up all registered processes"""
        with self._cleanup_lock:
            if self._is_cleaned_up:
                return

            self.logger.info(f"🧹 Cleaning up processes on port {self.port}")

            # Stop processes directly with fast force kill
            for process_name, process in self.processes.items():
                if process:
                    self.logger.info(
                        f"🧹 Stopping {process_name} process (PID: {process.pid if process.poll() is None else 'terminated'})"
                    )
                    try:
                        if process.poll() is None:
                            self._force_kill_process(process, process_name)
                    except Exception as e:
                        self.logger.verbose(f"Error during {process_name} cleanup: {e}")

            # Clean up port
            self._cleanup_port(self.port)

            # Mark as cleaned up
            self._is_cleaned_up = True
            self.logger.info(f"🧹 Process cleanup complete")

    def _cleanup_port(self, port: int):
        """Clean up processes using the specified port"""
        try:
            # Kill processes using the port
            subprocess.run(
                f"lsof -ti:{port} | xargs -r kill -9", shell=True, capture_output=True
            )
        except Exception:
            pass

    def _force_kill_process(self, process: subprocess.Popen, process_name: str):
        """Fast force kill process - no waiting needed for tests"""
        try:
            process.kill()
        except Exception as e:
            self.logger.verbose(f"Exception during {process_name} force kill: {e}")

    def __del__(self):
        """Ensure cleanup on destruction"""
        if not self._is_cleaned_up:
            self.cleanup_processes()
