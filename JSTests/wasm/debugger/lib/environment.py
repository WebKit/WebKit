"""
WebKit build environment detection for WebAssembly debugger tests.
"""

import os
import subprocess
from pathlib import Path
from .utils import log_info, log_warning, log_error, log_success, log_verbose


class WebKitEnvironment:
    """Detects JSC and LLDB paths and sets up DYLD_FRAMEWORK_PATH."""

    def __init__(self, script_path: Path, build_config: str = None):
        # script_path is test-wasm-debugger.py
        # debugger/ -> wasm/ -> JSTests/ -> OpenSource/ (webkit_root)
        self.webkit_root = script_path.resolve().parent.parent.parent.parent
        self.build_config = build_config

        self.vm_path, self.jsc_path = self._find_jsc_path()

        self.env = os.environ.copy()
        self.env["VM"] = str(self.vm_path)
        self.env["DYLD_FRAMEWORK_PATH"] = str(self.vm_path)

        self.lldb_path = self._find_lldb_path()

    def _find_jsc_path(self):
        if self.build_config:
            vm_path = self.webkit_root / f"WebKitBuild/{self.build_config}"
            jsc_path = vm_path / "jsc"
            if jsc_path.exists():
                log_info(f"Found JSC at: {jsc_path}")
            else:
                log_error(f"JSC not found at: {jsc_path}")
                log_info(f"Build with: Tools/Scripts/build-webkit --{self.build_config.lower()}")
            return vm_path, jsc_path

        for config in ("WebKitBuild/Debug", "WebKitBuild/Release"):
            vm_path = self.webkit_root / config
            jsc_path = vm_path / "jsc"
            if jsc_path.exists():
                log_info(f"Found JSC at: {jsc_path}")
                return vm_path, jsc_path

        default_vm = self.webkit_root / "WebKitBuild/Debug"
        default_jsc = default_vm / "jsc"
        log_warning(f"JSC not found, using default path: {default_jsc}")
        return default_vm, default_jsc

    def _find_lldb_path(self) -> str:
        try:
            result = subprocess.run(
                ["xcrun", "--find", "lldb"],
                capture_output=True, text=True, check=True, timeout=5,
            )
            path = result.stdout.strip()
            if path and Path(path).exists():
                return path
        except (subprocess.CalledProcessError, subprocess.TimeoutExpired, FileNotFoundError):
            pass
        return "lldb"

    def validate(self) -> bool:
        valid = True
        if not self.jsc_path.exists():
            log_error(f"JSC binary not found at {self.jsc_path}")
            log_info("Build WebKit: Tools/Scripts/build-webkit --debug")
            valid = False
        else:
            log_success(f"JSC binary found at {self.jsc_path}")

        if self.lldb_path == "lldb":
            log_info("Using system LLDB")
        elif not Path(self.lldb_path).exists():
            log_warning(f"LLDB not found at {self.lldb_path}, falling back to system LLDB")
            self.lldb_path = "lldb"
        else:
            log_success(f"LLDB found at {self.lldb_path}")

        return valid
