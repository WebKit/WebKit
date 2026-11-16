#!/usr/bin/env python3
"""Record every swift-frontend job a `swiftc` invocation runs, with timestamps.

`-stats-output-dir` cannot see the whole story: under `-explicit-module-build`
the driver first runs a dependency scan, then compiles all the module
dependencies it discovered (`generate-pcm`, `compile-module-from-interface`), and
only then compiles your sources. Those module jobs write no stats file, because
the driver synthesizes their command lines from the scanner's output instead of
inheriting the frontend flags -- so they show up as a large unexplained gap
between a ninja edge starting and its first source compile.

The driver *will* announce every job it spawns if asked with `-parseable-output`,
so this script wraps the compiler, injects that flag, timestamps each `began` /
`finished` message as it arrives, and appends one JSON line per event to a log
that ninja_build_trace.py can read.

Use it in place of the compiler, passing the real one the same way WebKit's
Tools/Scripts/swift/swiftc-wrapper.py takes it. CMAKE_Swift_COMPILER_LAUNCHER is
not an option: CMake never binds ${LAUNCHER} for Swift compile edges, and
CMakeSwiftInformation.cmake unconditionally clears CMAKE_Swift_COMPILER_ARG1.

    mkdir -p /tmp/swiftstats
    cmake -B build -G Ninja \
        -DCMAKE_Swift_COMPILER=/path/to/swiftc_job_recorder.py \
        -DCMAKE_Swift_FLAGS="-stats-output-dir /tmp/swiftstats \
            --original-swift-compiler=$(xcrun --find swiftc) \
            --jobs-log=/tmp/swift-jobs.jsonl" ...
    ninja -C build

or directly:

    swiftc_job_recorder.py --original-swift-compiler=/usr/bin/swiftc \
        --jobs-log=/tmp/swift-jobs.jsonl -c foo.swift

Both flags may appear anywhere in the command line and are consumed rather than
forwarded, so they can ride along in CMAKE_Swift_FLAGS.
`--original-swift-compiler=` defaults to `swiftc` from PATH when absent, matching
the wrapper -- which matters because CMake's static-archive rule expands neither
<FLAGS> nor link options, so a flag in CMAKE_Swift_FLAGS never reaches it.
With no --jobs-log it execs the compiler unchanged, so leaving the recorder
configured costs nothing (and configure-time probes stay untouched).

Timestamps come from when each message reaches this process, which means nothing
between here and the driver may buffer stderr.
"""

from __future__ import annotations

import json
import os
import subprocess
import sys
import time

MESSAGE_KEYS = ("kind", "name", "pid")


def log_line(handle, payload: dict) -> None:
    """Append one JSON line. Concurrent ninja edges share this file, so the line
    is written with a single write() to an O_APPEND handle to stay intact."""
    handle.write(json.dumps(payload, separators=(",", ":")) + "\n")
    handle.flush()


def first_output(message: dict) -> str | None:
    outputs = message.get("outputs") or []
    if outputs and isinstance(outputs[0], dict):
        return outputs[0].get("path")
    return None


def batch_key(message: dict) -> str | None:
    """Identify the frontend process a batch-mode job ran in.

    swift-driver compiles several primaries in one frontend process, then reports
    each of them separately under a negative quasi-PID, every one carrying that
    process's whole span. Left ungrouped they read as N concurrent processes.
    Each constituent repeats the batch's full command line, so its first
    -primary-file names the process they share.
    """
    if (message.get("pid") or 0) >= 0:
        return None
    args = message.get("command_arguments") or []
    primaries = [a for flag, a in zip(args, args[1:]) if flag == "-primary-file"]
    return primaries[0] if primaries else None


def read_messages(stream, on_message, on_text) -> None:
    """Consume swift-driver's parseable output: a byte count, a newline, then
    that many bytes of JSON. Anything else is ordinary compiler output."""
    while True:
        line = stream.readline()
        if not line:
            return
        stripped = line.strip()
        if stripped.isdigit():
            length = int(stripped)
            body = b""
            while len(body) < length:
                chunk = stream.read(length - len(body))
                if not chunk:
                    break
                body += chunk
            try:
                on_message(json.loads(body.decode("utf-8", "replace")))
                continue
            except ValueError:
                on_text(body.decode("utf-8", "replace"))
                continue
        elif stripped:
            on_text(line.decode("utf-8", "replace"))


def parse_args(argv: list[str]) -> tuple[str, str | None, list[str]]:
    """Pull out our own flags and return (compiler, jobs_log, compiler_args).

    `--original-swift-compiler=` matches the convention WebKit's own
    Tools/Scripts/swift/swiftc-wrapper.py uses: the flag may appear anywhere in
    the command line, is consumed rather than forwarded, and defaults to `swiftc`
    from PATH when absent. That default matters, because CMake's static-archive
    rule (`CMAKE_Swift_CREATE_STATIC_LIBRARY`) expands neither `<FLAGS>` nor link
    options, so a flag carried in CMAKE_Swift_FLAGS never reaches it.
    """
    compiler = "swiftc"
    jobs_log: str | None = None
    compiler_args: list[str] = []
    for arg in argv:
        if arg.startswith("--original-swift-compiler="):
            compiler = arg[len("--original-swift-compiler="):]
        elif arg.startswith("--jobs-log="):
            jobs_log = arg[len("--jobs-log="):]
        else:
            compiler_args.append(arg)
    return compiler, jobs_log, compiler_args


def main(argv: list[str]) -> int:
    compiler, jobs_log, compiler_args = parse_args(argv)

    if not jobs_log:
        os.execvp(compiler, [compiler] + compiler_args)  # nothing to record

    command = [compiler] + compiler_args
    if "-parseable-output" not in command:
        command.append("-parseable-output")

    handle = open(jobs_log, "a", buffering=1)
    recorder_pid = os.getpid()
    started = time.time()
    log_line(
        handle,
        {
            "t": started,
            "kind": "driver-began",
            "rec": recorder_pid,
            "cwd": os.getcwd(),
            "compiler": compiler,
        },
    )

    process = subprocess.Popen(command, stdout=sys.stdout, stderr=subprocess.PIPE)

    def on_message(message: dict) -> None:
        record: dict = {"t": time.time(), "rec": recorder_pid}
        record.update({k: message.get(k) for k in MESSAGE_KEYS})
        path = first_output(message)
        if path:
            record["output"] = path
        inputs = message.get("inputs") or []
        if inputs:
            record["inputs"] = inputs
        batch = batch_key(message)
        if batch:
            record["batch"] = batch
        if message.get("kind") == "finished":
            record["exit_status"] = message.get("exit-status")
        log_line(handle, record)
        # -parseable-output moves diagnostics into the message body, so they
        # must be re-emitted or the build log loses every warning and error.
        text = message.get("output")
        if text:
            sys.stderr.write(text)
            sys.stderr.flush()

    def on_text(text: str) -> None:
        sys.stderr.write(text)
        sys.stderr.flush()

    read_messages(process.stderr, on_message, on_text)
    returncode = process.wait()
    log_line(
        handle,
        {
            "t": time.time(),
            "kind": "driver-finished",
            "rec": recorder_pid,
            "exit_status": returncode,
        },
    )
    handle.close()
    return returncode


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
