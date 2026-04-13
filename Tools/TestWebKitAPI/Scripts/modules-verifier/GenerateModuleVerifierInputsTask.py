import collections.abc
import pathlib
import shlex
import textwrap

import ModuleVerifierLanguage
from ModuleVerifierLanguage import Language, LanguageStandard
from ModuleVerifierTypes import (
    ModuleVerifierInputs,
    ModuleVerifierTarget,
    ModuleVerifierTargetSet,
)


def _module_verifier_target_set_combinations(
    targets: list[ModuleVerifierTarget],
    languages: list[Language],
    standards: list[LanguageStandard],
) -> list[ModuleVerifierTargetSet]:
    result: list[ModuleVerifierTargetSet] = []

    languages_and_standards = [
        (language, language.supported_standards(standards)) for language in languages
    ]

    for target in targets:
        # FIXME: Support Catalyst targets.
        if target.suffix is not None and "macabi" in target.suffix:
            continue

        for language, supported_standards in languages_and_standards:
            for standard in supported_standards:
                result.append(ModuleVerifierTargetSet(target, language, standard))

    return result


class GenerateModuleVerifierInputsTask:
    """
    A type used to generate inputs from an environment, for a module verifier to later consume.

    Each task describes a set of inputs which, when combined, form the following structure:

    ```
    {target_temp_dir}/
    └── VerifyModule/
        └── {product_name}/
            ├── {language}/
            │   ├── Test.{extension}
            │   └── Test.framework/
            │       ├── Headers/
            │       │   └── Test.h
            │       └── Modules/
            │           └── module.modulemap
            └── {language_1}/
                ├── ...
                └── ...
    ```
    """

    target_set: ModuleVerifierTargetSet
    inputs: ModuleVerifierInputs

    @classmethod
    def create_tasks(
        cls, environment: collections.abc.Mapping[str, str]
    ) -> list["GenerateModuleVerifierInputsTask"]:
        """
        Creates a collection of `ModuleVerifierInputGeneratorTask`s using the specified environment.

        The environment variables this routine depends on are:

        ```
        MODULE_VERIFIER_SUPPORTED_LANGUAGES
        MODULE_VERIFIER_SUPPORTED_LANGUAGE_STANDARDS
        ARCHS
        LLVM_TARGET_TRIPLE_VENDOR
        LLVM_TARGET_TRIPLE_OS_VERSION
        LLVM_TARGET_TRIPLE_SUFFIX
        PRODUCT_NAME
        TARGET_TEMP_DIR
        ```

        Args:
            environment: Specifies the types and combinations of tasks that should be created.
        Returns:
            The collection of input generator tasks; one for each language in `MODULE_VERIFIER_SUPPORTED_LANGUAGES`.
        Raises:
            AssertionError: If an invalid language or standard is specified, or an invalid combination thereof.
        """

        language_names = shlex.split(
            environment.get("MODULE_VERIFIER_SUPPORTED_LANGUAGES", "")
        )
        languages: list[Language] = []
        for name in language_names:
            try:
                languages.append(Language(name))
            except ValueError:
                raise AssertionError(f"Unsupported module verifier language {name}")

        standard_names = shlex.split(
            environment.get("MODULE_VERIFIER_SUPPORTED_LANGUAGE_STANDARDS", "")
        )
        standards: list[LanguageStandard] = []
        for name in standard_names:
            try:
                standards.append(LanguageStandard(name))
            except ValueError:
                raise AssertionError(
                    f"Unsupported module verifier language standard {name}"
                )

        ModuleVerifierLanguage.verify_languages(languages, standards)

        arch_names = shlex.split(environment.get("ARCHS", ""))

        vendor = environment["LLVM_TARGET_TRIPLE_VENDOR"]
        os_version = environment["LLVM_TARGET_TRIPLE_OS_VERSION"]
        suffix = environment.get("LLVM_TARGET_TRIPLE_SUFFIX")

        targets = [
            ModuleVerifierTarget(arch, vendor, os_version, suffix)
            for arch in arch_names
        ]

        product_name = environment["PRODUCT_NAME"]
        target_temp_dir = environment["TARGET_TEMP_DIR"]

        languages_to_inputs: dict[Language, ModuleVerifierInputs] = {}

        for language in languages:
            output_path = pathlib.Path(
                f"{target_temp_dir}/VerifyModule/{product_name}/{language.value}"
            )
            inputs = ModuleVerifierInputs(
                output_path / f"Test.{language.source_file_extension()}",
                output_path / "Test.framework" / "Headers" / "Test.h",
                output_path / "Test.framework" / "Modules" / "module.modulemap",
                output_path,
            )

            languages_to_inputs[language] = inputs

        target_sets = _module_verifier_target_set_combinations(
            targets, languages, standards
        )

        result: list[GenerateModuleVerifierInputsTask] = []

        for target_set in target_sets:
            inputs = languages_to_inputs[target_set.language]
            result.append(GenerateModuleVerifierInputsTask(target_set, inputs))

        return result

    def __init__(
        self, target_set: ModuleVerifierTargetSet, inputs: ModuleVerifierInputs
    ):
        self.target_set = target_set
        self.inputs = inputs

    def perform_action(self, headers: list[str]) -> None:
        """
        Generates the module verifier inputs described by this task, using the specified list of header files.

        The input test framework contains a module map that uses `Test.h` as an umbrella header, which itself includes all files in `headers`. This framework is then included by the translation unit file created for each valid language.

        This routine creates parent directories for each newly created file, if needed.

        Args:
            headers: A list of header files to include when generating the test header input. Each element in this list should be a string of the form `product_name/file.h`.
        """

        assert headers, "The list of headers must not be empty"

        for input_directory in (
            self.inputs.main,
            self.inputs.header,
            self.inputs.module_map,
        ):
            input_directory.parent.mkdir(parents=True, exist_ok=True)

        include = self.target_set.language.include_statement()

        self.inputs.main.write_text(f"{include} <Test/Test.h>")

        output = "\n".join(f"{include} <{header}>" for header in headers)
        self.inputs.header.write_text(output)

        module_map_contents = textwrap.dedent("""\
        framework module Test {
            umbrella header "Test.h"

            export *
            module * { export * }
        }
        """)

        self.inputs.module_map.write_text(module_map_contents)
