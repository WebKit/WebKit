from enum import Enum


class LanguageStandard(Enum):
    C_99 = "c99"
    GNU_99 = "gnu99"
    C_11 = "c11"
    GNU_11 = "gnu11"
    C_17 = "c17"
    GNU_17 = "gnu17"
    C_23 = "c23"
    GNU_23 = "gnu23"

    @classmethod
    def c_standards(cls: type["LanguageStandard"]) -> set["LanguageStandard"]:
        return {cls.C_99, cls.GNU_99, cls.C_11, cls.GNU_11, cls.C_17, cls.GNU_17, cls.C_23, cls.GNU_23}

    C_PLUS_PLUS_17 = "c++17"
    GNU_PLUS_PLUS_17 = "gnu++17"
    C_PLUS_PLUS_20 = "c++20"
    GNU_PLUS_PLUS_20 = "gnu++20"
    C_PLUS_PLUS_2B = "c++2b"
    GNU_PLUS_PLUS_2B = "gnu++2b"
    C_PLUS_PLUS_23 = "c++23"
    GNU_PLUS_PLUS_23 = "gnu++23"
    C_PLUS_PLUS_2C = "c++2c"
    GNU_PLUS_PLUS_2C = "gnu++2c"
    C_PLUS_PLUS_26 = "c++26"
    GNU_PLUS_PLUS_26 = "gnu++26"

    @classmethod
    def cxx_standards(cls) -> set["LanguageStandard"]:
        return {
            cls.C_PLUS_PLUS_17,
            cls.GNU_PLUS_PLUS_17,
            cls.C_PLUS_PLUS_20,
            cls.GNU_PLUS_PLUS_20,
            cls.C_PLUS_PLUS_2B,
            cls.GNU_PLUS_PLUS_2B,
            cls.C_PLUS_PLUS_23,
            cls.GNU_PLUS_PLUS_23,
            cls.C_PLUS_PLUS_2C,
            cls.GNU_PLUS_PLUS_2C,
            cls.C_PLUS_PLUS_26,
            cls.GNU_PLUS_PLUS_26,
        }


class Language(Enum):
    C = "c"
    C_PLUS_PLUS = "c++"
    OBJECTIVE_C = "objective-c"
    OBJECTIVE_C_PLUS_PLUS = "objective-c++"

    def supported_standards(
        self, standards: list[LanguageStandard]
    ) -> set[LanguageStandard]:
        if self == Language.C or self == Language.OBJECTIVE_C:
            return LanguageStandard.c_standards().intersection(standards)

        if self == Language.C_PLUS_PLUS or self == Language.OBJECTIVE_C_PLUS_PLUS:
            return LanguageStandard.cxx_standards().intersection(standards)

        assert False, "Unreachable code."

    def include_statement(self) -> str:
        if self == Language.C or self == Language.C_PLUS_PLUS:
            return "#include"

        if self == Language.OBJECTIVE_C or self == Language.OBJECTIVE_C_PLUS_PLUS:
            return "#import"

        assert False, "Unreachable code."

    def source_file_extension(self) -> str:
        if self == Language.C:
            return "c"

        if self == Language.C_PLUS_PLUS:
            return "cpp"

        if self == Language.OBJECTIVE_C:
            return "m"

        if self == Language.OBJECTIVE_C_PLUS_PLUS:
            return "mm"

        assert False, "Unreachable code."


def verify_languages(languages: list[Language], standards: list[LanguageStandard]) -> None:
    for language in languages:
        if not language.supported_standards(standards):
            raise AssertionError(f"No standard in '{standards}' is valid for language '{language.value}'")
