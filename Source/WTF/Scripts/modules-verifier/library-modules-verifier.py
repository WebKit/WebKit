#!/usr/bin/env python3

import argparse
from dataclasses import dataclass
import json
import os
import pathlib
import sys
from typing import TypedDict

from GenerateModuleVerifierInputsTask import GenerateModuleVerifierInputsTask
from VerifyModuleTask import VerifyModuleTask


@dataclass
class CommandArguments:
    tapi_filelist: pathlib.Path
    relative_to: pathlib.Path


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

    args = parser.parse_args()
    return CommandArguments(args.tapi_filelist, args.relative_to)


if __name__ == "__main__":
    if os.environ.get("ENABLE_WK_LIBRARY_MODULE_VERIFIER") != "YES":
        print(
            "warning: Library module verifier is not enabled. Set `ENABLE_WK_LIBRARY_MODULE_VERIFIER` to `YES` to enable the verifier."
        )
        sys.exit()

    arguments = parse_command_arguments()

    with open(arguments.tapi_filelist, "r") as tapi_filelist:
        filelist_data: FileList = json.load(tapi_filelist)

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

    for input_task in input_tasks:
        verify_task = VerifyModuleTask(
            input_task.target_set, input_task.inputs, os.environ
        )
        command = verify_task.create_command()

        print(
            f"Verifying clang module ({verify_task.language.value}, {verify_task.standard.value}, {verify_task.target}) ..."
        )
        print(" ".join(command))

        result = verify_task.perform_action()
        print(result.stderr)

        if result.returncode:
            sys.exit("error: Failed to verify module.")

    print("Verified module!")
    sys.exit()
