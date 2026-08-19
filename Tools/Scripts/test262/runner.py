#!/usr/bin/env python3

# Copyright (C) 2018 Bocoup LLC. All rights reserved.
# Copyright (C) 2026 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1. Redistributions of source code must retain the above
#    copyright notice, this list of conditions and the following
#    disclaimer.
# 2. Redistributions in binary form must reproduce the above
#    copyright notice, this list of conditions and the following
#    disclaimer in the documentation and/or other materials
#    provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
# OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
# TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
# THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.

"""Test262 test runner for WebKit's JavaScriptCore."""

import argparse
import html as html_module
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time
from pathlib import Path
from typing import Any, Dict, List, NoReturn, Optional, Set, Tuple

# Make webkitcorepy/webkitpy importable
_MODULE_DIR = Path(__file__).resolve().parent
_SCRIPTS_DIR = _MODULE_DIR.parent
_WEBKIT_ROOT = _SCRIPTS_DIR.parent.parent
for _lib_path in (str(_SCRIPTS_DIR), str(_SCRIPTS_DIR / "libraries" / "webkitcorepy")):
    if _lib_path not in sys.path:
        sys.path.insert(0, _lib_path)

from webkitcorepy import TaskPool, Version  # noqa: E402
from webkitpy.common.system.executive import Executive  # noqa: E402
from webkitpy.common.system.filesystem import FileSystem  # noqa: E402
from webkitpy.common.system.platforminfo import PlatformInfo  # noqa: E402
from webkitpy.common.webkit_finder import WebKitFinder  # noqa: E402

try:
    import yaml
    from yaml.events import AliasEvent, ScalarEvent
except ImportError:
    print(
        "Error: PyYAML is required. Install with: pip3 install pyyaml",
        file=sys.stderr,
    )
    sys.exit(1)

try:
    _YamlLoader = yaml.CSafeLoader
except AttributeError:
    _YamlLoader = yaml.SafeLoader


class _YamlDumper(yaml.SafeDumper):
    analysis: Any

    def check_simple_key(self) -> bool:
        if isinstance(self.event, AliasEvent):
            return True
        if isinstance(self.event, ScalarEvent):
            if self.analysis is None:
                self.analysis = self.analyze_scalar(self.event.value)
            return not self.analysis.empty and not self.analysis.multiline
        return self.check_empty_sequence() or self.check_empty_mapping()

    def choose_scalar_style(self) -> str:
        # When PyYAML would emit a string as single-quoted (e.g. because of
        # a `: ` that blocks plain style), and the string itself contains a
        # single quote, prefer double-quoted style so we get
        # `"...'foo'..."` rather than the ugly `'...''foo''...'` doubling.
        style = super().choose_scalar_style()
        if style == "'" and "'" in self.event.value:
            return '"'
        return style


# --- Path Constants ---

_DEFAULT_TEST262_DIR = _WEBKIT_ROOT / "JSTests" / "test262"
_DEFAULT_CONFIG = _DEFAULT_TEST262_DIR / "config.yaml"
_DEFAULT_EXPECTATIONS = _DEFAULT_TEST262_DIR / ("expectations-linux.yaml" if sys.platform.startswith("linux") else "expectations.yaml")
_REPORT_CSS = _MODULE_DIR / "report.css"

DEFAULT_HARNESS_FILES = ["sta.js", "assert.js"]
ASYNC_HARNESS_FILES = ["doneprintHandle.js"]

_METADATA_RE = re.compile(
    r"/\*(---[\r\n]+[\S\s]*)[\r\n]+\s*---\*/", re.MULTILINE
)
_ERROR_RE = re.compile(r"^Exception: ([\w\d]+: .*)", re.MULTILINE)
_TEST_FILE_RE = re.compile(r"(?<!_FIXTURE)\.[jJ][sS]$")


# --- Worker Process State ---

# Set once per worker by _worker_setup (which TaskPool runs inside each worker
# process) and read by _run_batch. A module global works across both fork and
# spawn start methods because the setup function runs in-process in every
# worker rather than relying on inherited parent state.
_WORKER_CTX: Optional[Dict[str, Any]] = None


def _worker_setup(ctx: Dict[str, Any]) -> None:
    global _WORKER_CTX
    _WORKER_CTX = ctx


def _run_batch(filepaths: List[str]) -> List[Dict[str, Any]]:
    assert _WORKER_CTX is not None
    results: List[Dict[str, Any]] = []
    for fp in filepaths:
        try:
            results.extend(_process_test_file(fp, _WORKER_CTX))
        except Exception as e:
            rel_path = os.path.relpath(fp, _WORKER_CTX["test262_dir"])
            results.append(
                {
                    "path": rel_path,
                    "mode": "default",
                    "time": 0,
                    "passed": False,
                    "error": f"Worker error: {e}",
                    "output": str(e),
                    "features": [],
                    "exit_code": 1,
                    "skipped": False,
                    "is_crash": True,
                }
            )
    return results


# --- Utilities ---


def _die(msg: str) -> NoReturn:
    print(f"Error: {msg}", file=sys.stderr)
    sys.exit(1)


def _load_yaml(path: str) -> Any:
    with open(path, "r", encoding="utf-8") as f:
        return yaml.load(f, Loader=_YamlLoader)


def _read_file(path: str) -> str:
    with open(path, "r", encoding="utf-8", errors="replace") as f:
        return f.read()


def _parse_metadata(contents: str, filepath: str) -> Dict[str, Any]:
    m = _METADATA_RE.search(contents)
    if not m:
        return {}
    try:
        return yaml.load(m.group(1), Loader=_YamlLoader) or {}
    except yaml.YAMLError as e:
        print(f"Warning: YAML parse error in {filepath}: {e}", file=sys.stderr)
        return {}


def _parse_error(output: Optional[str]) -> Optional[str]:
    if not output:
        return None
    m = _ERROR_RE.search(output)
    if m:
        return m.group(1)
    return output.split("\n")[0]


# --- Platform Detection ---


def _detect_platform(release: bool, sanitizer: Optional[str]) -> Dict[str, str]:
    info = PlatformInfo()
    # Normalize webkitpy's short os names to the labels used in config.yaml's
    # skip conditions (comparisons are case-insensitive, but keep them tidy).
    os_label = {"mac": "macOS", "linux": "Linux", "win": "Windows"}.get(
        info.os_name, info.os_name
    )
    return {
        "os": os_label,
        "os_version": str(info.os_version) if info.os_version else "",
        "build": "release" if release else "debug",
        "sanitizer": sanitizer or "",
    }


# --- Skip / Filter Logic ---


def _evaluate_condition(condition: Dict[str, str], platform_info: Dict[str, str]) -> bool:
    for key, value in condition.items():
        if key == "os":
            if platform_info["os"].lower() != value.lower():
                return False
        elif key == "os_version":
            if not platform_info["os_version"]:
                return False
            try:
                if not Version.from_string(platform_info["os_version"]).matches(str(value)):
                    return False
            except (ValueError, TypeError):
                return False
        elif key == "build":
            if platform_info["build"] != value.lower():
                return False
        elif key == "sanitizer":
            if platform_info["sanitizer"].lower() != value.lower():
                return False
    return True


def _should_skip(
    rel_path: str,
    metadata: Dict[str, Any],
    config: Dict[str, Any],
    skip_set: Set[str],
    skip_paths: List[str],
    skip_features_set: Set[str],
    filter_features: Set[str],
    platform_info: Dict[str, str],
    conditional_skips: List[Dict[str, Any]],
) -> bool:
    skip_section = config.get("skip") if config else None
    if skip_section:
        if rel_path in skip_set:
            return True

        for pattern in skip_paths:
            if re.search(pattern, rel_path):
                return True

        test_features = metadata.get("features") or []
        for feature in test_features:
            if feature in skip_features_set:
                if not filter_features or feature not in filter_features:
                    return True

        if filter_features:
            if not any(f in filter_features for f in test_features):
                return True
    elif filter_features:
        test_features = metadata.get("features") or []
        if not any(f in filter_features for f in test_features):
            return True

    for cond in conditional_skips:
        if_clause = cond.get("if", {})
        if not _evaluate_condition(if_clause, platform_info):
            continue
        cond_files = set(cond.get("files") or [])
        if rel_path in cond_files:
            return True
        cond_paths = cond.get("paths") or []
        for pattern in cond_paths:
            if re.search(pattern, rel_path):
                return True
        cond_features = set(cond.get("features") or [])
        test_features = metadata.get("features") or []
        for feature in test_features:
            if feature in cond_features:
                if not filter_features or feature not in filter_features:
                    return True

    return False


# --- Scenarios ---


def _get_scenarios(flags: List[str]) -> List[str]:
    if not flags:
        return ["strict mode", "default"]
    if "raw" in flags:
        return ["raw"]
    if "noStrict" in flags:
        return ["default"]
    if "onlyStrict" in flags:
        return ["strict mode"]
    if "module" in flags:
        return ["module"]
    return ["strict mode", "default"]


# --- Harness ---


def _compile_harness(filenames: List[str], harness_dir: str) -> str:
    content = ""
    for name in filenames:
        path = os.path.join(harness_dir, name)
        with open(path, "r", encoding="utf-8", errors="replace") as f:
            content += f.read()
    return content


def _compile_includes(
    includes: List[str], harness_dir: str, temp_dir: str
) -> Optional[str]:
    content = ""
    for name in includes:
        path = os.path.join(harness_dir, name)
        try:
            with open(path, "r", encoding="utf-8", errors="replace") as f:
                content += f.read()
        except FileNotFoundError:
            pass
    if not content:
        return None
    fd, path = tempfile.mkstemp(dir=temp_dir, suffix=".js")
    with os.fdopen(fd, "w", encoding="utf-8") as f:
        f.write(content)
    return path


def _get_feature_flags(config: Dict[str, Any], metadata: Dict[str, Any]) -> List[str]:
    flags = []
    flag_map = config.get("flags") if config else None
    if flag_map and metadata.get("features"):
        for feature in metadata["features"]:
            flag_name = flag_map.get(feature)
            if flag_name:
                flags.append(f"--{flag_name}=1")
    test_flags = metadata.get("flags") or []
    if "CanBlockIsFalse" in test_flags:
        flags.append("--can-block-is-false")
    return flags


# --- JSC Execution ---


def _run_jsc(
    jsc: str,
    filepath: str,
    scenario: str,
    metadata: Dict[str, Any],
    feature_flags: List[str],
    default_harness_file: Optional[str],
    async_harness_file: Optional[str],
    includes_file: Optional[str],
    timeout: Optional[int],
    env: Dict[str, str],
) -> Tuple[int, Optional[str], float]:
    cmd = [jsc]
    cmd.extend(feature_flags)

    if timeout:
        cmd.append(f"--watchdog={timeout}")

    negative = metadata.get("negative")
    if negative and negative.get("type"):
        cmd.append(f"--exception={negative['type']}")

    test_flags = metadata.get("flags") or []
    is_async = "async" in test_flags

    if is_async:
        cmd.append("--test262-async")

    if scenario != "raw" and default_harness_file:
        cmd.append(default_harness_file)

    if is_async and scenario != "raw" and async_harness_file:
        cmd.append(async_harness_file)

    if includes_file:
        cmd.append(includes_file)

    if scenario == "module":
        cmd.append(f"--module-file={filepath}")
    elif scenario == "strict mode":
        cmd.append(f"--strict-file={filepath}")
    else:
        cmd.append(filepath)

    process_timeout = (timeout / 1000 + 60) if timeout else 300

    start = time.monotonic()
    try:
        proc = subprocess.run(
            cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            env=env,
            timeout=process_timeout,
        )
        elapsed = time.monotonic() - start
        # Python reports signal kills as negative returncodes; normalize to
        # the shell convention (128+N) so `& 0x7f` crash detection works.
        returncode = proc.returncode
        if returncode < 0:
            returncode = 128 + (-returncode)
        if returncode != 0:
            # Match Perl's `chomp`: remove trailing newlines but preserve
            # trailing spaces on the last line (some Test262Error messages
            # have a meaningful trailing space).
            output = (proc.stdout or "").rstrip("\n\r")
            return returncode, output if output else None, elapsed
        return 0, None, elapsed
    except subprocess.TimeoutExpired:
        elapsed = time.monotonic() - start
        return 1, "Timeout", elapsed
    except Exception as e:
        elapsed = time.monotonic() - start
        return 1, str(e), elapsed


# --- Test File Processing (Worker) ---


def _process_test_file(
    filepath: str, ctx: Dict[str, Any]
) -> List[Dict[str, Any]]:
    test262_dir = ctx["test262_dir"]
    rel_path = os.path.relpath(filepath, test262_dir)

    try:
        contents = _read_file(filepath)
    except Exception as e:
        return [
            {
                "path": rel_path,
                "mode": "default",
                "time": 0,
                "passed": False,
                "error": f"Read error: {e}",
                "output": str(e),
                "features": [],
                "exit_code": 1,
                "skipped": False,
                "is_crash": True,
            }
        ]

    metadata = _parse_metadata(contents, filepath)
    features = metadata.get("features") or []

    skip = _should_skip(
        rel_path,
        metadata,
        ctx["config"],
        ctx["skip_set"],
        ctx["skip_paths"],
        ctx["skip_features"],
        ctx["filter_features"],
        ctx["platform_info"],
        ctx["conditional_skips"],
    )

    if ctx["skipped_only"]:
        skip = not skip

    if skip:
        if not ctx["skipped_only"]:
            return [
                {
                    "path": rel_path,
                    "mode": "skip",
                    "time": 0,
                    "passed": False,
                    "error": None,
                    "output": None,
                    "features": features,
                    "exit_code": 0,
                    "skipped": True,
                    "is_crash": False,
                }
            ]
        return []

    scenarios = _get_scenarios(metadata.get("flags") or [])
    feature_flags = _get_feature_flags(ctx["config"], metadata)

    includes = metadata.get("includes") or []
    includes_file = None
    if includes:
        includes_file = _compile_includes(
            includes, ctx["harness_dir"], ctx["temp_dir"]
        )

    results = []
    for scenario in scenarios:
        exit_code, output, exec_time = _run_jsc(
            ctx["jsc"],
            filepath,
            scenario,
            metadata,
            feature_flags,
            ctx["default_harness_file"],
            ctx["async_harness_file"],
            includes_file,
            ctx["timeout"],
            ctx["env"],
        )

        error = None
        passed = True
        is_crash = False

        exit_signal = exit_code & 0x7f
        if exit_signal == 3:
            exit_signal = 0
        is_crash = exit_signal != 0

        if output or is_crash:
            error = _parse_error(output) if output else None
            if is_crash:
                error = f"Bad exit code: {exit_code}"
            passed = False

        results.append(
            {
                "path": rel_path,
                "mode": scenario,
                "time": exec_time,
                "passed": passed,
                "error": error,
                "output": output if not passed else None,
                "features": features,
                "exit_code": exit_code,
                "skipped": False,
                "is_crash": is_crash,
            }
        )

    if includes_file:
        try:
            os.unlink(includes_file)
        except OSError:
            pass

    return results


# --- Result Classification ---


def _classify_results(
    raw_results: List[Dict[str, Any]],
    expectations: Optional[Dict[str, Any]],
) -> List[Dict[str, Any]]:
    classified = []
    for r in raw_results:
        if r["skipped"]:
            r["result"] = "skip"
            classified.append(r)
            continue

        path = r["path"]
        mode = r["mode"]
        expected_exit = None
        if expectations and path in expectations:
            expected_exit = expectations[path].get(mode)

        if not r["passed"]:
            is_new_failure = not (
                expectations is not None
                and expected_exit is not None
                and r.get("exit_code") == expected_exit
            )
            r["result"] = "unexpected_fail" if is_new_failure else "expected_fail"
        else:
            r["result"] = "unexpected_pass" if expected_exit is not None else "expected_pass"

        classified.append(r)
    return classified


# --- Test Discovery ---


def _discover_tests(test262_dir: str, test_dirs: List[str]) -> List[str]:
    files = []
    for test_dir in test_dirs:
        root = os.path.join(test262_dir, test_dir)
        # --test-only accepts a single test file as well as a directory.
        if os.path.isfile(root):
            if _TEST_FILE_RE.search(os.path.basename(root)):
                files.append(root)
            else:
                print(f"Warning: not a test file: {root}", file=sys.stderr)
            continue
        if not os.path.isdir(root):
            print(f"Warning: test path not found: {root}", file=sys.stderr)
            continue
        for dirpath, _, filenames in os.walk(root):
            for f in sorted(filenames):
                if _TEST_FILE_RE.search(f):
                    files.append(os.path.join(dirpath, f))
    return files


def _load_import_file(test262_dir: str) -> List[str]:
    import_file = os.path.join(test262_dir, "latest-changes-summary.txt")
    if not os.path.exists(import_file):
        _die(f"Import file not found: {import_file}")

    files = []
    with open(import_file, "r") as f:
        for line in f:
            line = line.strip()
            if line and line[0] in ("A", "M") and "test/" in line:
                parts = line.split(None, 1)
                if len(parts) == 2:
                    files.append(os.path.join(test262_dir, parts[1]))
    return files


def _find_failing(results_file: str, test262_dir: str) -> List[str]:
    if not os.path.exists(results_file):
        _die(f"Results file not found: {results_file}")
    all_results = _load_yaml(results_file)
    if not all_results:
        _die("No results found in results file.")
    paths = set()
    for r in all_results:
        if r.get("result", "").endswith("fail"):
            paths.add(r["path"])
    return [os.path.join(test262_dir, p) for p in paths]


# --- JSC Finder ---


# Disambiguates which jsc binary to run.
_PORT_BUILD_DIR_NAMES: Dict[str, str] = {"gtk": "GTK", "wpe": "WPE", "jsc-only": "JSCOnly"}


def _find_jsc(jsc_path: Optional[str], release: bool, port: Optional[str] = None) -> str:
    if jsc_path:
        jsc = os.path.abspath(jsc_path)
        if not os.path.exists(jsc):
            _die(f"--jsc path does not exist: {jsc}")
        return jsc

    config_name = "Release" if release else "Debug"
    finder = WebKitFinder(FileSystem())

    if port:
        # These ports build the executable into a "bin" subdirectory rather
        # than directly into the configuration directory.
        config_dir = Path(finder.path_from_webkit_outputdir(_PORT_BUILD_DIR_NAMES[port], config_name))
        candidates = [config_dir / "bin" / "jsc"]
    else:
        # WebKitFinder.path_from_webkit_outputdir honors WEBKIT_OUTPUTDIR and
        # otherwise falls back to <webkit>/WebKitBuild.
        config_dir = Path(finder.path_from_webkit_outputdir(config_name))
        candidates = [
            config_dir / "bin" / "jsc",
            config_dir / "jsc",
            # Apple ports ship jsc inside the framework, see jscPath() in
            # webkitdirs.pm and the equivalent probe in run-jsc-stress-tests.
            config_dir / "JavaScriptCore.framework" / "Helpers" / "jsc",
            config_dir / "Debug" / "jsc",
            config_dir / "Release" / "jsc",
        ]

    for c in candidates:
        if c.exists():
            return str(c)

    if port:
        _die(
            f"Cannot find jsc in {config_dir}.\n"
            "Build it, or specify an existing binary with --jsc <path>."
        )

    try:
        result = subprocess.run(
            ["which", "jsc"], capture_output=True, text=True
        )
        if result.returncode == 0 and result.stdout.strip():
            return result.stdout.strip()
    except Exception:
        pass

    _die(
        f"Cannot find jsc in {config_dir}.\n"
        "Try --release or specify with --jsc <path>."
    )


# --- Save Expectations ---


# Written at the top of expectations.yaml on every save.
_EXPECTATIONS_HEADER = """\
# Expected test262 failures. Generated by Tools/Scripts/test262-runner --save.
#
# Each entry maps a test file to the modes it is expected to fail in ("default",
# "strict mode", "module" or "raw"), and each mode to the exit code jsc is
# expected to terminate with:
#
#   3        The test ran to completion and failed: an assertion in the test or
#            harness failed, or the test threw an uncaught exception. This is
#            jsc's EXIT_EXCEPTION.
#   128 + N  jsc was killed by signal N, i.e. it crashed. Common values are 134
#            (SIGABRT, which includes assertion failures in debug builds), 138
#            (SIGBUS) and 139 (SIGSEGV).
#   1        jsc could not run the test at all, or the runner timed out waiting
#            for it.
#
# A failing test counts as expected only if it fails with the exit code recorded
# here, so a test that starts crashing where it previously failed an assertion
# is still reported as a new failure.
"""


def _save_expectations(
    expectations_file: str, results: List[Dict[str, Any]], running_all: bool
) -> None:
    failed: Dict[str, Dict[str, int]] = {}
    for r in results:
        if r["result"].endswith("fail"):
            failed.setdefault(r["path"], {})[r["mode"]] = r["exit_code"]

    if not running_all:
        old: Dict[str, Dict[str, int]] = {}
        if os.path.exists(expectations_file):
            old = _load_yaml(expectations_file) or {}

        run_tests: Dict[str, Set[str]] = {}
        for r in results:
            if not r["skipped"]:
                run_tests.setdefault(r["path"], set()).add(r["mode"])

        for path, modes in run_tests.items():
            if path in old:
                for mode in modes:
                    old[path].pop(mode, None)
                if not old[path]:
                    del old[path]

        for path, mode_exits in failed.items():
            old.setdefault(path, {}).update(mode_exits)

        failed = old

    with open(expectations_file, "w", encoding="utf-8") as f:
        f.write(_EXPECTATIONS_HEADER)
        yaml.dump(
            failed,
            f,
            Dumper=_YamlDumper,
            default_flow_style=False,
            allow_unicode=True,
            sort_keys=True,
            explicit_start=True,
            width=2**31 - 1,
        )
    print(f"Saved expectation file in: {expectations_file}")


# --- Save Results ---


def _save_results(results_file: str, results: List[Dict[str, Any]]) -> None:
    data = []
    for r in results:
        entry = {
            "path": r["path"],
            "mode": r["mode"],
            "time": r.get("time", 0),
            "result": r["result"],
        }
        if r.get("error"):
            entry["error"] = r["error"]
        if r.get("output"):
            entry["output"] = r["output"]
        if r.get("features"):
            entry["features"] = r["features"]
        data.append(entry)

    with open(results_file, "w", encoding="utf-8") as f:
        yaml.dump(
            data,
            f,
            Dumper=_YamlDumper,
            default_flow_style=False,
            allow_unicode=True,
            explicit_start=True,
            width=2**31 - 1,
        )
    print(f"Saved all the results in {results_file}")


# --- Summary Generation ---


def _summarize_results(
    results: List[Dict[str, Any]], results_dir: str
) -> None:
    print("Summarizing results...")
    os.makedirs(results_dir, exist_ok=True)

    summary_txt = os.path.join(results_dir, "summary.txt")
    summary_yaml = os.path.join(results_dir, "summary.yaml")
    summary_html = os.path.join(results_dir, "summary.html")

    by_feature: Dict[str, List[float]] = {}
    by_path: Dict[str, List[float]] = {}

    for test in results:
        result = test.get("result", "skip")
        test_time = test.get("time", 0) or 0

        if test.get("features"):
            for feature in test["features"]:
                stats = by_feature.setdefault(feature, [0, 0, 0, 0.0])
                if result.endswith("pass"):
                    stats[0] += 1
                if result.endswith("fail"):
                    stats[1] += 1
                if result == "skip":
                    stats[2] += 1
                stats[3] += test_time

        parts = test["path"].split("/")
        # Skip the first element ("test") and the filename
        folder_parts = parts[1:-1]
        for i in range(len(folder_parts)):
            partial = "/".join(folder_parts[: i + 1])
            stats = by_path.setdefault(partial, [0, 0, 0, 0.0])
            if result.endswith("pass"):
                stats[0] += 1
            if result.endswith("fail"):
                stats[1] += 1
            if result == "skip":
                stats[2] += 1
            stats[3] += test_time

    # Write summary.txt
    with open(summary_txt, "w") as f:
        f.write(
            f"{'TOTAL':<6} {'RUN':<6} {'PASS-%':<6} {'PASS':<6} "
            f"{'FAIL':<6} {'SKIP':<6} {'TIME':<7} {'AVG':<6} FEATURE\n"
        )
        for key in sorted(by_feature):
            s = by_feature[key]
            total_run = s[0] + s[1]
            total = total_run + s[2]
            pct = f"{(s[0] / total * 100):.0f}%" if total else "0%"
            t = f"{s[3]:.1f}s"
            avg = f"{s[3] / total_run:.2f}s" if total_run else "0s"
            f.write(
                f"{total:<6} {total_run:<6} {pct:<6} {s[0]:<6} "
                f"{s[1]:<6} {s[2]:<6} {t:<7} {avg:<6} {key}\n"
            )

        f.write(
            f"\n\n{'TOTAL':<6} {'RUN':<6} {'PASS-%':<6} {'PASS':<6} "
            f"{'FAIL':<6} {'SKIP':<6} {'TIME':<7} {'AVG':<6} FOLDER\n"
        )
        for key in sorted(by_path):
            s = by_path[key]
            total_run = s[0] + s[1]
            total = total_run + s[2]
            pct = f"{(s[0] / total * 100):.0f}%" if total else "0%"
            t = f"{s[3]:.1f}s"
            avg = f"{s[3] / total_run:.2f}s" if total_run else "0s"
            f.write(
                f"{total:<6} {total_run:<6} {pct:<6} {s[0]:<6} "
                f"{s[1]:<6} {s[2]:<6} {t:<7} {avg:<6} {key}\n"
            )

    # Write summary.html
    with open(summary_html, "w") as f:
        f.write(
            '<html><head><title>Test262 Summaries</title>\n'
            '<link rel="stylesheet" href="report.css">\n'
            "</head><body>\n"
            "<h1>Test262 Summaries</h1>\n"
            '<div class="visit">Visit <a href="index.html">the index</a>'
            " for a report of failures.</div>\n"
            "<h2>By Features</h2>\n"
            '<table class="summary-table"><thead>\n'
            "<th>Feature</th><th>%</th><th>Total</th><th>Run</th>"
            "<th>Passed</th><th>Failed</th><th>Skipped</th>"
            "<th>Exec. time</th><th>Avg. time</th>\n"
            "</thead><tbody>\n"
        )
        for key in sorted(by_feature):
            s = by_feature[key]
            total_run = s[0] + s[1]
            total = total_run + s[2]
            iper = (s[0] / total * 100) if total else 0
            pct = f"{iper:.0f}%"
            t = f"{s[3]:.1f}s"
            avg = f"{s[3] / total_run:.2f}s" if total_run else "0s"
            f.write(
                f'<tr class="per-{iper}">'
                f"<td>{html_module.escape(key)}</td><td>{pct}</td>"
                f"<td>{total}</td><td>{total_run}</td>"
                f"<td>{s[0]}</td><td>{s[1]}</td><td>{s[2]}</td>"
                f"<td>{t}</td><td>{avg}</td></tr>\n"
            )

        f.write(
            "</tbody></table>\n"
            "<h2>By Path</h2>\n"
            '<table class="summary-table"><thead>\n'
            "<th>Folder</th><th>%</th><th>Total</th><th>Run</th>"
            "<th>Passed</th><th>Failed</th><th>Skipped</th>"
            "<th>Exec. time</th><th>Avg. time</th>\n"
            "</thead><tbody>\n"
        )
        for key in sorted(by_path):
            s = by_path[key]
            total_run = s[0] + s[1]
            total = total_run + s[2]
            iper = (s[0] / total * 100) if total else 0
            pct = f"{iper:.0f}%"
            t = f"{s[3]:.1f}s"
            avg = f"{s[3] / total_run:.2f}s" if total_run else "0s"
            f.write(
                f'<tr class="per-{iper}">'
                f"<td>{html_module.escape(key)}</td><td>{pct}</td>"
                f"<td>{total}</td><td>{total_run}</td>"
                f"<td>{s[0]}</td><td>{s[1]}</td><td>{s[2]}</td>"
                f"<td>{t}</td><td>{avg}</td></tr>\n"
            )

        f.write(
            "</tbody></table>\n"
            '<div class="visit">Visit <a href="index.html">the index</a>'
            " for a report of failures.</div>\n"
            "</body></html>\n"
        )

    # Write summary.yaml
    summary_data = {
        "byFeature": {k: v[:] for k, v in by_feature.items()},
        "byFolder": {k: v[:] for k, v in by_path.items()},
    }
    with open(summary_yaml, "w", encoding="utf-8") as f:
        yaml.dump(
            summary_data,
            f,
            Dumper=_YamlDumper,
            default_flow_style=False,
            allow_unicode=True,
            width=2**31 - 1,
        )


# --- HTML Index Report ---


def _generate_html_index(
    failed: Dict[str, Dict[str, Any]],
    results_dir: str,
    total_run: int,
    fail_count: int,
    new_fail_count: int,
    skip_count: int,
) -> None:
    os.makedirs(results_dir, exist_ok=True)
    index_html = os.path.join(results_dir, "index.html")

    with open(index_html, "w") as f:
        f.write(
            '<html><head><title>Test262 Results</title>\n'
            '<link rel="stylesheet" href="report.css">\n'
            "</head><body>\n"
            "<h1>Test262 Results</h1>\n"
            '<div class="visit">Visit <a href="summary.html">the summary</a>'
            " for statistics.</div>\n"
            "<h2>Stats</h2><ul>\n"
        )

        failed_files = len(failed)
        total_plus = total_run + skip_count
        f.write(
            f"<li>{total_run} test files run from {total_plus} files, "
            f"{skip_count} skipped test files</li>\n"
            f"<li>{fail_count} failures from {failed_files} distinct files, "
            f"{new_fail_count} new failures</li>\n"
        )

        f.write("</ul><h2>Failures</h2><ul>\n")

        for path in sorted(failed):
            scenarios = failed[path]
            escaped_path = html_module.escape(path)
            f.write(
                f'<li class="list-item">'
                f'<label for="{escaped_path}" class="expander-control">'
                f"{escaped_path}</label>\n"
                f'<input type="checkbox" id="{escaped_path}" class="expander">\n'
                f'<ul class="expand">\n'
            )
            for scenario, value in sorted(scenarios.items()):
                escaped_value = html_module.escape(str(value)) if value else ""
                f.write(
                    f"<li>{html_module.escape(scenario)}: {escaped_value}</li>\n"
                )
            f.write("</ul></li>\n")

        f.write(
            "</ul>\n"
            '<div class="visit">Visit <a href="summary.html">the summary</a>'
            " for statistics.</div>\n"
            "</body></html>\n"
        )


# --- CLI ---


def _parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Test262 runner for WebKit's JavaScriptCore."
    )
    parser.add_argument(
        "-j", "--jsc", help="Path to JSC binary."
    )
    parser.add_argument(
        "-t", "--t262", help="Root test262 directory."
    )
    parser.add_argument(
        "-o",
        "--test-only",
        action="append",
        dest="test_only",
        help="Run specific test262 subdirectory (can be repeated).",
    )
    parser.add_argument(
        "-p",
        "--child-processes",
        type=int,
        dest="child_processes",
        help="Number of child processes for parallel execution.",
    )

    build_group = parser.add_mutually_exclusive_group()
    build_group.add_argument(
        "--release", action="store_true", default=False, help="Use Release build of JSC (default)."
    )
    build_group.add_argument(
        "--debug", action="store_true", default=False, help="Use Debug build of JSC."
    )

    port_group = parser.add_mutually_exclusive_group()
    port_group.add_argument(
        "--gtk", action="store_const", const="gtk", dest="port",
        help="Find jsc under the GTK port's build directory (WebKitBuild/GTK).",
    )
    port_group.add_argument(
        "--wpe", action="store_const", const="wpe", dest="port",
        help="Find jsc under the WPE port's build directory (WebKitBuild/WPE).",
    )
    port_group.add_argument(
        "--jsc-only", action="store_const", const="jsc-only", dest="port",
        help="Find jsc under the JSCOnly port's build directory (WebKitBuild/JSCOnly).",
    )

    parser.add_argument(
        "-v", "--verbose", action="store_true", help="Verbose output."
    )
    parser.add_argument(
        "-f",
        "--features",
        action="append",
        help="Filter tests by feature (can be repeated).",
    )
    parser.add_argument(
        "-c", "--config", help="Path to config file."
    )
    parser.add_argument(
        "-i",
        "--ignore-config",
        action="store_true",
        dest="ignore_config",
        help="Ignore config file.",
    )
    parser.add_argument(
        "--save",
        action="store_true",
        help="Save current failures to expectations file.",
    )
    parser.add_argument(
        "-e", "--expectations", help="Path to expectations file."
    )
    parser.add_argument(
        "-x",
        "--ignore-expectations",
        action="store_true",
        dest="ignore_expectations",
        help="Ignore expectations file; report all failures.",
    )
    parser.add_argument(
        "-F",
        "--failing-files",
        action="store_true",
        dest="failing_files",
        help="Re-run only tests that failed in a previous results file.",
    )
    parser.add_argument(
        "-l",
        "--latest-import",
        action="store_true",
        dest="latest_import",
        help="Run only files from the latest import.",
    )
    parser.add_argument(
        "--stats",
        action="store_true",
        help="Calculate statistics from a results file.",
    )
    parser.add_argument(
        "-r", "--results", help="Path to results file (for --stats or --failing-files)."
    )
    parser.add_argument(
        "--timeout", type=int, help="Timeout per test in milliseconds."
    )
    parser.add_argument(
        "-S",
        "--skipped-files",
        action="store_true",
        dest="skipped_files",
        help="Run only tests skipped by config.",
    )
    parser.add_argument(
        "--no-progress",
        action="store_true",
        dest="no_progress",
        help="Suppress progress output.",
    )

    # --- New features ---
    parser.add_argument(
        "--filter",
        help="Regex filter on test paths. Only tests whose path matches are run.",
    )
    parser.add_argument(
        "--sanitizer",
        help="Declare sanitizer in use (e.g. asan, tsan) for conditional skip rules.",
    )

    args = parser.parse_args()
    # A build configuration is optional and defaults to Release, matching the
    # original runner. --release/--debug are mutually exclusive, so --debug is
    # the only thing that moves us off Release; everything downstream reads
    # args.release.
    args.release = not args.debug
    return args


# --- Main ---


def main() -> None:
    args = _parse_args()
    results_dir = os.path.join(os.getcwd(), "test262-results")

    # --- Stats-only mode ---
    if args.stats:
        results_file = args.results or os.path.join(results_dir, "results.yaml")
        if not os.path.exists(results_file):
            _die(f"Results file not found: {results_file}")
        results = _load_yaml(results_file)
        if not results:
            _die("No results in results file.")
        _summarize_results(results, results_dir)
        return

    # --- Failing-files: validate results file early ---
    if args.failing_files:
        results_file = args.results or os.path.join(results_dir, "results.yaml")
        if not os.path.exists(results_file):
            _die("Cannot find results file. Specify with --results.")

    # --- Find JSC ---
    jsc = _find_jsc(args.jsc, args.release, args.port)

    # --- Resolve paths ---
    if args.latest_import:
        test262_dir = str(_DEFAULT_TEST262_DIR)
    elif args.t262:
        test262_dir = os.path.abspath(args.t262)
    else:
        test262_dir = str(_DEFAULT_TEST262_DIR)

    harness_dir = os.path.join(test262_dir, "harness")
    if not os.path.exists(harness_dir):
        harness_dir = str(_DEFAULT_TEST262_DIR / "harness")

    config_file = args.config or str(_DEFAULT_CONFIG)
    expectations_file = args.expectations or str(_DEFAULT_EXPECTATIONS)
    if args.expectations:
        expectations_file = os.path.abspath(args.expectations)

    # --- Load config ---
    config: Dict[str, Any] = {}
    if not args.ignore_config:
        if os.path.exists(config_file):
            config = _load_yaml(config_file) or {}
        elif args.config:
            _die(f"Config file {config_file} does not exist.")

    # --- Load expectations ---
    expectations = None
    if (
        not args.ignore_expectations
        and not args.skipped_files
        and os.path.exists(expectations_file)
    ):
        expectations = _load_yaml(expectations_file) or {}

    # --- Platform info ---
    platform_info = _detect_platform(args.release, args.sanitizer)

    # --- Build skip sets ---
    skip_set: Set[str] = set()
    skip_paths: List[str] = []
    skip_features: Set[str] = set()
    conditional_skips: List[Dict[str, Any]] = []

    if config and "skip" in config:
        skip_section = config["skip"]
        skip_set = set(skip_section.get("files") or [])
        skip_paths = skip_section.get("paths") or []
        skip_features_raw = skip_section.get("features") or []
        skip_features = set()
        for f in skip_features_raw:
            m = re.match(r"(\S+)", str(f))
            if m:
                skip_features.add(m.group(1))
        conditional_skips = skip_section.get("conditions") or []

    filter_features = set(args.features) if args.features else set()

    # --- Environment ---
    dyld_path = os.path.dirname(os.path.abspath(jsc))
    env = os.environ.copy()
    if "TZ" not in env:
        env["TZ"] = "PST"
    if platform_info["os"] == "macOS":
        env["DYLD_FRAMEWORK_PATH"] = dyld_path
    elif platform_info["os"] == "Linux":
        lib_dir = os.path.normpath(os.path.join(dyld_path, os.pardir, "lib"))
        existing_ld_path = env.get("LD_LIBRARY_PATH")
        env["LD_LIBRARY_PATH"] = (
            f"{lib_dir}{os.pathsep}{existing_ld_path}" if existing_ld_path else lib_dir
        )

    # --- Process count ---
    num_processes = args.child_processes or Executive().cpu_count() or 1

    # --- Print settings ---
    print(f"\nSettings:")
    print(f"Test262 Dir: {os.path.relpath(test262_dir)}")
    print(f"JSC: {os.path.relpath(jsc)}")
    print(f"Child Processes: {num_processes}")
    if args.timeout:
        print(f"Test timeout: {args.timeout}ms")
    if args.port:
        print(f"Port: {args.port}")
    if platform_info["os"] == "macOS":
        print(f"DYLD_FRAMEWORK_PATH: {dyld_path}")
    elif platform_info["os"] == "Linux":
        print(f"LD_LIBRARY_PATH: {env['LD_LIBRARY_PATH']}")
    if filter_features:
        print(f"Features to include: {', '.join(sorted(filter_features))}")
    if args.test_only:
        print(f"Paths: {', '.join(args.test_only)}")
    if config:
        print(f"Config file: {os.path.relpath(config_file)}")
    if expectations:
        print(f"Expectations file: {os.path.relpath(expectations_file)}")
    if args.filter:
        print(f"Filter: {args.filter}")
    if args.sanitizer:
        print(f"Sanitizer: {args.sanitizer}")
    if args.failing_files:
        print("Running only the failing files in results")
    if args.latest_import:
        print("Running only the latest imported files")
    if args.skipped_files:
        print("Running only the skipped files in config.yaml")
    if args.verbose:
        print("Verbose mode")
    print("---\n")

    # --- Create temp directory and run ---
    with tempfile.TemporaryDirectory() as temp_dir:
        # Compile harness
        default_content = _compile_harness(DEFAULT_HARNESS_FILES, harness_dir)
        default_harness_file = os.path.join(temp_dir, "default_harness.js")
        with open(default_harness_file, "w", encoding="utf-8") as f:
            f.write(default_content)

        async_content = _compile_harness(ASYNC_HARNESS_FILES, harness_dir)
        async_harness_file = os.path.join(temp_dir, "async_harness.js")
        with open(async_harness_file, "w", encoding="utf-8") as f:
            f.write(async_content)

        # --- Discover test files ---
        test_dirs = args.test_only or ["test"]
        running_all = (
            not args.test_only
            and not args.latest_import
            and not args.failing_files
            and not args.filter
            and not filter_features
        )

        if args.latest_import:
            files = _load_import_file(test262_dir)
        elif args.failing_files:
            rf = args.results or os.path.join(results_dir, "results.yaml")
            files = _find_failing(rf, test262_dir)
        else:
            files = _discover_tests(test262_dir, test_dirs)

        # Apply regex filter
        if args.filter:
            try:
                pattern = re.compile(args.filter)
            except re.error as e:
                _die(f"Invalid --filter regex: {e}")
            files = [
                f
                for f in files
                if pattern.search(os.path.relpath(f, test262_dir))
            ]

        if not files:
            # Running nothing is never a success: exiting 0 here makes a typo in
            # --test-only or --filter look like a clean run.
            print("No test files found.", file=sys.stderr)
            sys.exit(1)

        total_files = len(files)

        # --- Build worker context ---
        ctx = {
            "test262_dir": test262_dir,
            "config": config,
            "harness_dir": harness_dir,
            "jsc": jsc,
            "default_harness_file": default_harness_file,
            "async_harness_file": async_harness_file,
            "timeout": args.timeout,
            "env": env,
            "skip_set": skip_set,
            "skip_paths": skip_paths,
            "skip_features": skip_features,
            "filter_features": filter_features,
            "skipped_only": args.skipped_files,
            "temp_dir": temp_dir,
            "platform_info": platform_info,
            "conditional_skips": conditional_skips,
            "verbose": args.verbose,
        }

        # --- Run tests ---
        start_time = time.monotonic()
        all_results: List[Dict[str, Any]] = []
        completed = 0
        last_print = 0.0
        # Let's not spam log files too hard
        progress_interval = 0.1 if sys.stdout.isatty() else 5.0

        # Batch files per task to amortize IPC/pickle overhead. Aim for ~4
        # batches per worker so there's still some load-balancing headroom.
        batch_size = max(1, min(50, total_files // (num_processes * 4) or 1))
        batches = [
            files[i:i + batch_size]
            for i in range(0, total_files, batch_size)
        ]

        def _print_progress(final: bool = False) -> None:
            elapsed = time.monotonic() - start_time
            rate = completed / elapsed if elapsed > 0 else 0
            remaining = total_files - completed
            eta = remaining / rate if rate > 0 else 0
            em, es = divmod(int(elapsed), 60)
            etm, ets = divmod(int(eta), 60)
            end_char = "\n" if final else ""
            line_char = "\r" if sys.stdout.isatty() else "\n"
            print(
                f"{line_char}[{completed}/{total_files}] "
                f"{rate:5.1f}/s  {em}m{es:02d}s elapsed  "
                f"ETA {etm}m{ets:02d}s   ",
                end=end_char,
                flush=True,
            )

        def _on_batch_done(results: List[Dict[str, Any]], batch_len: int) -> None:
            nonlocal completed, last_print
            all_results.extend(results)
            completed += batch_len
            if not args.no_progress:
                now = time.monotonic()
                if now - last_print >= progress_interval or completed >= total_files:
                    _print_progress(final=completed >= total_files)
                    last_print = now

        # TaskPool runs _worker_setup once per worker to seed the per-worker
        # context, then invokes each batch's callback back in this process (so
        # progress printing and result collection stay single-threaded). Its
        # context manager terminates workers on exit, including when a
        # KeyboardInterrupt unwinds through it.
        try:
            with TaskPool(
                workers=num_processes,
                name="test262",
                setup=_worker_setup,
                setupargs=(ctx,),
            ) as pool:
                for batch in batches:
                    pool.do(
                        _run_batch,
                        batch,
                        callback=lambda results, n=len(batch): _on_batch_done(results, n),
                    )
                pool.wait()
        except KeyboardInterrupt:
            if not args.no_progress:
                print()
            print("\nInterrupted. Exiting...", file=sys.stderr)
            sys.exit(130)

        # --- Sort and classify results ---
        all_results.sort(key=lambda r: (r["path"], r["mode"]))
        all_results = _classify_results(all_results, expectations)

        # --- Compute statistics ---
        failed: Dict[str, Dict[str, Any]] = {}
        fail_count = 0
        new_fail_count = 0
        new_pass_count = 0
        skip_count = 0
        new_failure_report = ""
        new_pass_report = ""

        for test in all_results:
            path = test["path"]
            mode = test["mode"]
            result = test["result"]

            if result.endswith("fail"):
                fail_count += 1
                failed.setdefault(path, {})[mode] = test.get("error")

                if result == "unexpected_fail":
                    new_fail_count += 1
                    output = test.get("output", "")
                    exit_code = test.get("exit_code", 0)
                    features_str = ""
                    if args.verbose and test.get("features"):
                        features_str = (
                            "Features: "
                            + ", ".join(test["features"])
                            + "\n"
                        )
                    msg = (
                        f"! NEW FAIL {path} ({mode}) "
                        f"(Exit code: {exit_code})\n"
                        f"{features_str}{output}\n"
                    )
                    if args.verbose:
                        new_failure_report += msg + "\n"
                    # --skipped-files runs the tests the config skips, which are
                    # expected to fail; reporting every one of them as a new
                    # failure is pure noise, so only --verbose asks for it.
                    suppressed = args.skipped_files and not args.verbose
                    if not suppressed and (not expectations or not args.verbose):
                        print(msg)

            elif result.endswith("pass"):
                is_new_pass = result == "unexpected_pass"
                # --skipped-files deliberately runs without an expectations
                # file, so nothing is ever classified unexpected_pass there.
                # Every test that passes is one the config no longer needs to
                # skip, which is the entire point of the mode.
                if is_new_pass or args.skipped_files:
                    new_pass_count += 1
                    new_pass_report += f"PASS {path} ({mode})\n"
                if is_new_pass:
                    if not args.verbose:
                        print(f"NEW PASS {path} ({mode})")
                elif args.verbose:
                    print(f"PASS {path} ({mode})")

            elif result == "skip":
                skip_count += 1
                if args.verbose:
                    print(f"SKIP {path}")

        # --- Print summaries ---
        if args.verbose and expectations and (new_failure_report or new_pass_report):
            print()
            if new_failure_report:
                print("---------------NEW FAILING TESTS SUMMARY---------------\n")
                print(new_failure_report)
            if new_pass_report:
                print("---------------NEW PASSING TESTS SUMMARY---------------\n")
                print(new_pass_report)
            print("---------------------------------------------------------\n")

        if args.skipped_files and new_pass_report:
            print("---------------NEW PASSING TESTS SUMMARY---------------")
            print(f"\n{new_pass_report}")
            print("---------------------------------------------------------")

        total_run = len(all_results) - skip_count
        print(f"\n{total_run} tests run")
        print(f"{skip_count} test files skipped")

        if not expectations:
            print(f"{fail_count} tests failed")
            if args.skipped_files:
                print(f"{new_pass_count} tests newly pass")
        else:
            print(f"{fail_count} tests failed in total")
            print(f"{new_fail_count} tests newly fail")
            print(f"{new_pass_count} tests newly pass")

        # --- Save expectations ---
        if args.save:
            _save_expectations(expectations_file, all_results, running_all)

        # --- Save results and reports ---
        os.makedirs(results_dir, exist_ok=True)
        results_file = os.path.join(results_dir, "results.yaml")
        _save_results(results_file, all_results)

        if _REPORT_CSS.exists():
            shutil.copy(str(_REPORT_CSS), results_dir)

        _summarize_results(all_results, results_dir)
        _generate_html_index(
            failed, results_dir, total_run, fail_count, new_fail_count, skip_count
        )

        elapsed = time.monotonic() - start_time
        print(f"See the summaries and results in the {results_dir}.")
        print(f"\nDone in {elapsed:.2f} seconds!")

        total_failures = new_fail_count if expectations else fail_count
        sys.exit(1 if total_failures else 0)
