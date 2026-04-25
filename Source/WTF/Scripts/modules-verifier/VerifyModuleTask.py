import collections.abc
import pathlib
import shlex
import subprocess

from ModuleVerifierLanguage import Language, LanguageStandard
from ModuleVerifierTypes import (
    ModuleVerifierInputs,
    ModuleVerifierTargetSet,
)


class VerifyModuleTask:
    """
    A type used to verify a module for correctness by compiling a test framework from a set of
    inputs with modules enabled.

    The test framework should contain include statements for each exposed header in the target
    library product. If any of the exposed headers are missing from the library module map, or
    if there are any other module-related issues in any header, running this task will indicate
    the verification has failed.
    """

    output_path: pathlib.Path
    target: str
    other_c_flags: list[str]
    other_verifier_flags: list[str]
    other_cxx_flags: list[str]
    language: Language
    standard: LanguageStandard
    sys_root: str
    framework_search_paths: list[str]
    source_file: str

    def __init__(
        self,
        target_set: ModuleVerifierTargetSet,
        inputs: ModuleVerifierInputs,
        environment: collections.abc.Mapping[str, str],
    ):
        """
        Creates a new VerifyModuleTask whose properties are derived using the specified target set, inputs, and environment.

        The environment variables this routine depends on are:

        ```
        PRODUCT_NAME
        TARGET_TEMP_DIR
        OTHER_CFLAGS
        OTHER_MODULE_VERIFIER_FLAGS
        OTHER_CPLUSPLUSFLAGS
        SDKROOT
        FRAMEWORK_SEARCH_PATHS
        ```

        Args:
            target_set: The target set used to derive the language, language standards, and target properties.
            inputs: The set of input paths used to verify the module.
            environment: Specifies the configuration options to use for the verification command.
        """

        product_name = environment["PRODUCT_NAME"]
        target_temp_dir = environment["TARGET_TEMP_DIR"]

        self.language = target_set.language
        self.standard = target_set.standard

        self.output_path = (
            pathlib.Path(target_temp_dir)
            / "VerifyModule"
            / product_name
            / target_set.path_component()
        )

        self.target = target_set.target.value()
        self.source_file = str(inputs.main)

        self.other_c_flags = shlex.split(environment.get("OTHER_CFLAGS", ""))
        self.other_c_flags += [
            f"-Wsystem-headers-in-module={product_name}",
            "-Werror=non-modular-include-in-module",
            "-Werror=non-modular-include-in-framework-module",
            "-Werror=incomplete-umbrella",
            "-Werror=quoted-include-in-framework-header",
            "-Werror=atimport-in-framework-header",
            "-Werror=framework-include-private-from-public",
            "-Werror=incomplete-framework-module-declaration",
            "-Wundef-prefix=TARGET_OS",
            "-Werror=undef-prefix",
            "-Werror=module-import-in-extern-c",
            "-ferror-limit=0",
        ]

        self.other_verifier_flags = shlex.split(
            environment.get("OTHER_MODULE_VERIFIER_FLAGS", "")
        )
        if len(self.other_verifier_flags) > 1:
            self.other_c_flags += self.other_verifier_flags[1:]

        self.other_cxx_flags = shlex.split(environment.get("OTHER_CPLUSPLUSFLAGS", ""))
        self.other_cxx_flags += ["-fcxx-modules"]
        self.other_cxx_flags += self.other_c_flags

        self.sys_root = environment["SDKROOT"]

        self.framework_search_paths = shlex.split(
            environment.get("FRAMEWORK_SEARCH_PATHS", "")
        )
        self.framework_search_paths += [str(inputs.directory)]

    def create_command(self) -> list[str]:
        """
        Creates the set of command arguments described by this task to be evaluated.

        Returns:
            A set of arguments describing a clang invocation, suitable to pass to a process to be run.
        """

        arguments: list[str] = []
        arguments += ["-x", self.language.value]
        arguments += ["-target", self.target]
        arguments += [f"-std={self.standard.value}"]
        arguments += [
            "-fmodules",
            "-fsyntax-only",
            "-fdiagnostics-show-note-include-stack",
        ]

        if (
            self.language == Language.C_PLUS_PLUS
            or self.language == Language.OBJECTIVE_C_PLUS_PLUS
        ):
            arguments += self.other_cxx_flags
        else:
            arguments += self.other_c_flags

        arguments += ["-isysroot", self.sys_root]

        for framework_search_path in self.framework_search_paths:
            arguments += [f"-F{framework_search_path}"]

        return ["xcrun", "clang"] + arguments + [self.source_file]

    def perform_action(self) -> subprocess.CompletedProcess[str]:
        """
        Verifies the module by running a clang command produced by this task.

        Returns:
            The result of running the command. If the return code of the result is `0`, the module succeeded in verification.
            Otherwise, the module failed verification, and `stderr` describes the errors.
        """

        command = self.create_command()
        return subprocess.run(command, stderr=subprocess.PIPE, text=True)
