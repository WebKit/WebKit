#!/usr/bin/env python3

import argparse
import concurrent.futures
import os
import pathlib
import shlex
import sys

from GenerateModuleVerifierInputsTask import GenerateModuleVerifierInputsTask
from VerifyModuleTask import VerifyModuleTask


def parse_command_arguments():
    parser = argparse.ArgumentParser(
        prog="framework-modules-verifier",
        description="Tests framework clang modules in a clean environment.",
    )

    parser.add_argument(
        "framework_path",
        type=pathlib.Path,
        help="A path to a framework bundle (for example, `path/to/JavaScriptCore.framework`).",
    )

    parser.add_argument(
        "--depfile",
        type=pathlib.Path,
        help="Path to write a Makefile-style discovered dependency file for Xcode.",
    )

    return parser.parse_args()


if __name__ == "__main__":
    arguments = parse_command_arguments()

    headers_dir = arguments.framework_path / "Headers"
    framework_name = arguments.framework_path.stem

    header_paths = sorted(headers_dir.glob("*.h"))

    # Always write the dependency file so Xcode can track header changes,
    # regardless of whether the verifier is enabled.
    if arguments.depfile:
        with open(arguments.depfile, "w") as depfile:
            depfile.write("dependencies:")
            for path in header_paths:
                depfile.write(f" \\\n  {shlex.quote(str(path))}")
            module_map = arguments.framework_path / "Modules" / "module.modulemap"
            if module_map.exists():
                depfile.write(f" \\\n  {shlex.quote(str(module_map))}")
            private_module_map = (
                arguments.framework_path / "Modules" / "module.private.modulemap"
            )
            if private_module_map.exists():
                depfile.write(f" \\\n  {shlex.quote(str(private_module_map))}")
            depfile.write("\n")

    if os.environ.get("ENABLE_WK_FRAMEWORK_MODULE_VERIFIER") != "YES":
        sys.exit()

    if not header_paths:
        print("warning: No headers found in framework.")
        sys.exit()

    # Construct framework-style include paths (e.g., "JavaScriptCore/JSBase.h").
    headers = [f"{framework_name}/{path.name}" for path in header_paths]

    print("Generating inputs for module verifier...")

    input_tasks = GenerateModuleVerifierInputsTask.create_tasks(os.environ)
    if not input_tasks:
        print("warning: No inputs were generated for the verifier.")
        sys.exit()

    for input_task in input_tasks:
        input_task.perform_action(headers)

    print("Generated inputs for module verifier!")

    verify_tasks = []
    for input_task in input_tasks:
        verify_task = VerifyModuleTask(
            input_task.target_set, input_task.inputs, os.environ
        )
        command = verify_task.create_command()

        print(
            f"Verifying clang module ({verify_task.language.value}, {verify_task.standard.value}, {verify_task.target}) ..."
        )
        print(" ".join(command))
        verify_tasks.append(verify_task)

    failed = False
    with concurrent.futures.ThreadPoolExecutor() as executor:
        futures = {executor.submit(task.perform_action): task for task in verify_tasks}
        for future in concurrent.futures.as_completed(futures):
            task = futures[future]
            result = future.result()
            if result.stderr:
                print(result.stderr)
            if result.returncode:
                failed = True

    if failed:
        sys.exit("error: Failed to verify module.")
