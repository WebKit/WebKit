# Copyright (C) 2026 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
# THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
# BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
# THE POSSIBILITY OF SUCH DAMAGE.

"""
webkit_auto_attach -- CodeLLDB auto-attach to WebKit XPC children.

Loaded via `postRunCommands` on a CodeLLDB launch of MiniBrowser (or any
WebKit2 client built into a CMake build dir). Spawns a daemon thread that
polls libproc for processes whose executable lives under this build's
WebKit.framework/XPCServices/, and for each new arrival fires CodeLLDB's
DAP `startDebugging` reverse-request so the child appears as a nested debug
session in VS Code's Call Stack.

Polling proc_pidpath at 50 ms gets the attach in before anything past
XPCServiceMain has run in practice, which is good enough for breakpoints in
WebCore/JSC; it cannot guarantee catching the very first instructions of
service entry the way a suspended-spawn debugger hook would.

Filtering by *executable path under the build dir* (rather than process name
or parent PID) means we never attach to system Safari's services or another
checkout's MiniBrowser -- only binaries produced by this exact ninja build.
"""

import ctypes
import os
import threading
import time

import lldb

POLL_INTERVAL_S = 0.05
PROC_ALL_PIDS = 1
PROC_PIDPATHINFO_MAXSIZE = 4096

_libproc = ctypes.CDLL("/usr/lib/libproc.dylib")
_libproc.proc_listpids.restype = ctypes.c_int
_libproc.proc_listpids.argtypes = [ctypes.c_uint32, ctypes.c_uint32, ctypes.c_void_p, ctypes.c_int]
_libproc.proc_pidpath.restype = ctypes.c_int
_libproc.proc_pidpath.argtypes = [ctypes.c_int, ctypes.c_void_p, ctypes.c_uint32]


def _list_pids():
    n = _libproc.proc_listpids(PROC_ALL_PIDS, 0, None, 0)
    if n <= 0:
        return []
    buf = (ctypes.c_int * (n // ctypes.sizeof(ctypes.c_int)))()
    n = _libproc.proc_listpids(PROC_ALL_PIDS, 0, buf, ctypes.sizeof(buf))
    return buf[: n // ctypes.sizeof(ctypes.c_int)]


def _pid_path(pid, _buf=ctypes.create_string_buffer(PROC_PIDPATHINFO_MAXSIZE)):
    n = _libproc.proc_pidpath(pid, _buf, PROC_PIDPATHINFO_MAXSIZE)
    return _buf.raw[:n].decode("utf-8", "replace") if n > 0 else ""


def _read_cmake_cache(build_dir, key):
    try:
        with open(os.path.join(build_dir, "CMakeCache.txt")) as f:
            for line in f:
                if line.startswith(key + ":"):
                    return line.split("=", 1)[1].rstrip()
    except OSError:
        pass
    return ""


def _display_name(exe_path, pid, counts):
    # com.apple.WebKit.WebContent.Development -> WebContent
    name = os.path.basename(exe_path)
    name = name.removeprefix("com.apple.WebKit.").removesuffix(".Development")
    n = counts[name] = counts.get(name, 0) + 1
    return f"{name} ({pid})" if n == 1 else f"{name} #{n} ({pid})"


class _Watcher:
    def __init__(self, debugger):
        try:
            from codelldb import interface as codelldb_interface
        except ImportError:
            print("webkit_auto_attach: codelldb module not available; "
                  "auto-attach only works under the CodeLLDB adapter.")
            return

        self._fire = codelldb_interface.fire_event
        self._debugger_id = debugger.GetID()

        target = debugger.GetSelectedTarget()
        process = target.GetProcess()
        self._ui_pid = process.GetProcessID()
        if self._ui_pid in (0, lldb.LLDB_INVALID_PROCESS_ID):
            self._log("no running process; load via postRunCommands, not initCommands.")
            return

        exe = target.GetExecutable()
        exe_path = os.path.join(exe.GetDirectory() or "", exe.GetFilename() or "")
        # Walk up from the executable until we hit the CMake build dir; covers
        # both bare tools (<build>/WebKitTestRunner) and bundles
        # (<build>/MiniBrowser.app/Contents/MacOS/MiniBrowser).
        probe = os.path.dirname(exe_path)
        for _ in range(4):
            if os.path.exists(os.path.join(probe, "CMakeCache.txt")):
                self._build_dir = probe
                break
            probe = os.path.dirname(probe)
        else:
            self._log(f"could not locate CMakeCache.txt above {exe_path}; disabled.")
            return

        self._xpc_prefix = os.path.join(
            self._build_dir, "WebKit.framework", "Versions", "A", "XPCServices") + os.sep
        source_dir = _read_cmake_cache(self._build_dir, "CMAKE_HOME_DIRECTORY")
        self._child_init = [f"command source {os.path.join(self._build_dir, 'lldbinit')}"]
        self._child_source_map = {".": source_dir, "build": self._build_dir} if source_dir else {}

        self._seen = set()
        self._counts = {}

        self._log(f"watching for XPC children of pid {self._ui_pid} under {self._xpc_prefix}")
        t = threading.Thread(target=self._run, name="webkit-auto-attach", daemon=True)
        t.start()

    def _log(self, msg):
        try:
            self._fire(self._debugger_id,
                       dict(type="DebuggerMessage", category="console",
                            output=f"[auto-attach] {msg}\n"))
        except Exception:
            print(f"[auto-attach] {msg}")

    def _ui_alive(self):
        try:
            os.kill(self._ui_pid, 0)
            return True
        except OSError:
            return False

    def _run(self):
        while self._ui_alive():
            try:
                self._scan()
            except Exception as e:  # noqa: BLE001 -- a watcher must not die.
                self._log(f"scan error: {e!r}")
            time.sleep(POLL_INTERVAL_S)
        self._log("UI process exited; stopping.")

    def _scan(self):
        live = set()
        for pid in _list_pids():
            if pid == 0 or pid == self._ui_pid:
                continue
            path = _pid_path(pid)
            if not path.startswith(self._xpc_prefix):
                continue
            live.add(pid)
            if pid in self._seen:
                continue
            self._seen.add(pid)
            self._attach(pid, path)
        # Forget dead ones so a same-PID-reuse (rare) or our own bookkeeping
        # doesn't grow unbounded over a long session.
        self._seen &= live

    def _attach(self, pid, path):
        name = _display_name(path, pid, self._counts)
        self._log(f"attaching: {name}")
        self._fire(self._debugger_id, dict(
            type="StartDebugging",
            request="attach",
            configuration=dict(
                name=name,
                pid=pid,
                stopOnEntry=False,
                initCommands=self._child_init,
                sourceMap=self._child_source_map,
                presentation=dict(group="auto-attach"),
            ),
        ))


def __lldb_init_module(debugger, internal_dict):
    internal_dict["__webkit_auto_attach"] = _Watcher(debugger)
