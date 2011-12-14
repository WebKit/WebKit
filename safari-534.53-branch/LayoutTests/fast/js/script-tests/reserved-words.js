function isReserved(word)
{
    try {
        eval("var " + word + ";");
        return false;
    } catch (e) {
        return true;
    }
}

var reservedWords = [
    "break",
    "case",
    "catch",
    "class",
    "const",
    "continue",
    "debugger",
    "default",
    "delete",
    "do",
    "else",
    "enum",
    "export",
    "extends",
    "false",
    "finally",
    "for",
    "function",
    "if",
    "import",
    "in",
    "instanceof",
    "new",
    "null",
    "return",
    "super",
    "switch",
    "this",
    "throw",
    "true",
    "try",
    "typeof",
    "var",
    "void",
    "while",
    "with"
];

var unreservedWords = [
    "abstract",
    "boolean",
    "byte",
    "char",
    "double",
    "final",
    "float",
    "goto",
    "implements",
    "int",
    "interface",
    "long",
    "native",
    "package",
    "private",
    "protected",
    "public",
    "short",
    "static",
    "synchronized",
    "throws",
    "transient",
    "volatile"
];

description(
"This file checks which ECMAScript 3 keywords are treated as reserved words."
);

reservedWords.sort();
unreservedWords.sort();

debug("SHOULD BE RESERVED:");
for (var p in reservedWords) {
    shouldBeTrue("isReserved('" + reservedWords[p] + "')");
}

debug("");

debug("SHOULD NOT BE RESERVED:");
for (var p in unreservedWords) {
    shouldBeFalse("isReserved('" + unreservedWords[p] + "')");
}

debug("");

var successfullyParsed = true;
