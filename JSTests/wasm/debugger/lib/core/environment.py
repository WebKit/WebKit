"""
WebKit build environment detection and setup for WebAssembly debugger tests
"""

import os
from pathlib import Path
from .utils import Logger


class WebKitEnvironment:
    """Manages WebKit build environment and paths"""

    def __init__(self, script_path: Path, build_config: str = None):
        # Calculate WebKit root from script location
        # Path: JSTests/wasm/debugger/test-wasm-debugger.py
        # We need to go up 3 levels to get to the WebKit root
        # debugger -> wasm -> JSTests -> OpenSource -> WebKit root
        self.webkit_root = script_path.resolve().parent.parent.parent.parent

        self.build_config = build_config

        # Try multiple build configurations or use specific one
        self.vm_path, self.jsc_path = self._find_jsc_path()

        # Set up environment variables
        self.env = os.environ.copy()
        self.env["VM"] = str(self.vm_path)
        self.env["DYLD_FRAMEWORK_PATH"] = str(self.vm_path)

        # LLDB paths (try hardcoded first, then system)
        self.lldb_path = self._find_lldb_path()

    def _find_jsc_path(self):
        """Find JSC binary in various build configurations"""
        if self.build_config:
            # Use specific build configuration
            specific_config = f"WebKitBuild/{self.build_config}"
            vm_path = self.webkit_root / specific_config
            jsc_path = vm_path / "jsc"
            if jsc_path.exists():
                Logger.info(
                    f"Found JSC at: {jsc_path} (using --{self.build_config.lower()} option)"
                )
                return vm_path, jsc_path
            else:
                Logger.error(f"JSC not found at specified path: {jsc_path}")
                Logger.info(f"To build WebKit in {self.build_config} mode:")
                Logger.info(
                    f"  Run: Tools/Scripts/build-webkit --{self.build_config.lower()}"
                )
                # Still return the path even if it doesn't exist, so the error is clear
                return vm_path, jsc_path

        # Auto-detect from multiple build configurations
        build_configs = [
            "WebKitBuild/Debug",
            "WebKitBuild/Release",
        ]

        for config in build_configs:
            vm_path = self.webkit_root / config
            jsc_path = vm_path / "jsc"
            if jsc_path.exists():
                Logger.info(f"Found JSC at: {jsc_path}")
                return vm_path, jsc_path

        # Default to Debug build path even if it doesn't exist
        default_vm_path = self.webkit_root / "WebKitBuild" / "Debug"
        default_jsc_path = default_vm_path / "jsc"
        Logger.warning(f"JSC not found, using default path: {default_jsc_path}")
        return default_vm_path, default_jsc_path

    def _find_lldb_path(self) -> str:
        """Find the best LLDB binary to use"""
        hardcoded_lldb = "/Users/yijiahuang/Library/Developer/Toolchains/swift-REBRANCH-DEVELOPMENT-SNAPSHOT-2025-09-25-a.xctoolchain/usr/bin/lldb"
        if Path(hardcoded_lldb).exists():
            return hardcoded_lldb
        return "lldb"  # Fall back to system LLDB

    def validate(self) -> bool:
        """Validate that required binaries exist"""
        valid = True

        if not self.jsc_path.exists():
            Logger.error(f"JSC binary not found at {self.jsc_path}")
            Logger.info("To build WebKit:")
            Logger.info("  1. Run: Tools/Scripts/build-webkit --debug")
            Logger.info("  2. Or run: Tools/Scripts/build-webkit --release")
            Logger.info("  3. This will create the JSC binary needed for testing")
            valid = False
        else:
            Logger.success(f"JSC binary found at {self.jsc_path}")

        # Check LLDB (non-critical)
        if self.lldb_path == "lldb":
            Logger.info("Using system LLDB")
        else:
            if Path(self.lldb_path).exists():
                Logger.success(f"Custom LLDB found at {self.lldb_path}")
            else:
                Logger.warning(
                    f"Custom LLDB not found at {self.lldb_path}, falling back to system LLDB"
                )
                self.lldb_path = "lldb"

        return valid

    def get_jsc_path(self) -> str:
        """Get the JSC executable path"""
        return str(self.jsc_path)

    def get_lldb_path(self) -> str:
        """Get the LLDB executable path"""
        return self.lldb_path
