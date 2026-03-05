import pathlib
from dataclasses import dataclass
from typing import Optional

from ModuleVerifierLanguage import Language, LanguageStandard


@dataclass
class ModuleVerifierTarget:
    architecture: str
    vendor: str
    os_version: str
    suffix: Optional[str]

    def value(self) -> str:
        return f"{self.architecture}-{self.vendor}-{self.os_version}{self.suffix or ''}"


@dataclass
class ModuleVerifierInputs:
    main: pathlib.Path
    header: pathlib.Path
    module_map: pathlib.Path
    directory: pathlib.Path


@dataclass
class ModuleVerifierTargetSet:
    target: ModuleVerifierTarget
    language: Language
    standard: LanguageStandard

    def path_component(self) -> str:
        return f"{self.language.value}-{self.standard.value}-{self.target.value()}"
