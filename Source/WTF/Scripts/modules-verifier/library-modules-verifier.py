#!/usr/bin/env python3

import argparse
import concurrent.futures
from dataclasses import dataclass
import json
import os
import pathlib
import shlex
import sys
from typing import Optional, TypedDict

from GenerateModuleVerifierInputsTask import GenerateModuleVerifierInputsTask
from VerifyModuleTask import VerifyModuleTask


@dataclass
class CommandArguments:
    tapi_filelist: pathlib.Path
    relative_to: pathlib.Path
    depfile: Optional[pathlib.Path]


class FileListHeader(TypedDict):
    path: pathlib.Path
    type: str


class FileList(TypedDict):
    headers: list[FileListHeader]
    version: str


def parse_command_arguments() -> CommandArguments:
    parser = argparse.ArgumentParser(
        prog="library-modules-verifier",
        description="Tests library clang modules in a clean environment.",
    )

    parser.add_argument(
        "tapi_filelist",
        type=pathlib.Path,
        help="A path to a tapi-installapi(1) compatible JSON file list containing the library headers (for example, `path/to/WTF.json`).",
    )

    parser.add_argument(
        "--relative-to",
        required=True,
        type=pathlib.Path,
        help="The destination location of the library (for example, `WebKitBuild/Debug/usr/local/include`)",
    )

    parser.add_argument(
        "--depfile",
        type=pathlib.Path,
        help="Path to write a Makefile-style discovered dependency file for Xcode.",
    )

    args = parser.parse_args()
    return CommandArguments(args.tapi_filelist, args.relative_to, args.depfile)


if __name__ == "__main__":
    arguments = parse_command_arguments()

    with open(arguments.tapi_filelist, "r") as tapi_filelist:
        filelist_data: FileList = json.load(tapi_filelist)

    header_paths = [header["path"] for header in filelist_data["headers"]]

    # Always write the dependency file so Xcode can track header changes,
    # regardless of whether the verifier is enabled.
    if arguments.depfile:
        with open(arguments.depfile, "w") as depfile:
            depfile.write("dependencies:")
            for path in header_paths:
                depfile.write(f" \\\n  {shlex.quote(str(path))}")
            depfile.write("\n")

    if os.environ.get("ENABLE_WK_LIBRARY_MODULE_VERIFIER") != "YES":
        print(
            "warning: Library module verifier is not enabled. Set `ENABLE_WK_LIBRARY_MODULE_VERIFIER` to `YES` to enable the verifier."
        )
        sys.exit()

    headers = [
        os.path.relpath(header["path"], arguments.relative_to)
        for header in filelist_data["headers"]
    ]

    print("Generating inputs for module verifier...")

    input_tasks = GenerateModuleVerifierInputsTask.create_tasks(os.environ)
    if not input_tasks:
        print("warning: No inputs were generated for the verifier.")
        sys.exit()

    for input_task in input_tasks:
        input_task.perform_action(headers)

    print("Generated inputs for module verifier!")

    verify_tasks: list[VerifyModuleTask] = []
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
