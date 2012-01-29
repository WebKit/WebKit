function isReserved(word)
{
    try {
        eval("\"use strict\";var " + word + ";");
        return false;
    } catch (e) {
    	var expectedError = "Use of reserved word '" + word + "' in strict mode";
    	if (expectedError == e.message)
        	return true;
        else
        	return false;
    }
}

var reservedWords = [
    "implements",
    "private",
    "public",
    "yield",
    "interface",
    "package",
    "protected",
    "static"
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
    "int",
    "long",
    "native",
    "short",
    "synchronized",
    "throws",
    "transient",
    "volatile"
];

description(
"This file checks which ECMAScript 3 keywords are treated as reserved words in strict mode."
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
