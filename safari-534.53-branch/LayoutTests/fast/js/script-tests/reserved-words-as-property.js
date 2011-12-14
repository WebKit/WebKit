description("Tests to ensure that we can use ES reserved words as property names.");

var reservedWords = ["true", "false", "null", "break", "case", "catch", "continue", "debugger", "default", "delete", "do", "else", "finally", "for",
                     "function", "if", "in", "instanceof", "new", "return", "switch", "this", "throw", "try", "typeof", "var", "void", "while", "with",
                     "class", "const", "enum", "export", "extends", "import", "super"];

function parseShouldThrow(str) {
    shouldThrow(str);
    shouldThrow("(function(){"+str+"})");
}

for (var i = 0; i < reservedWords.length; i++) {
    parseShouldThrow("var " + reservedWords[i]);
    parseShouldThrow("function g(" + reservedWords[i] + "){}");
    parseShouldThrow("try{}catch(" + reservedWords[i] + "){}");
    parseShouldThrow("function " + reservedWords[i] + "(){}");
}

var literal = "({";
for (var i = 0; i < reservedWords.length; i++)
    literal += reservedWords[i] + ": true,"; 
literal += " parsed: true })";

var obj;
shouldBeTrue("(obj=" + literal + ").parsed");
for (var i = 0; i < reservedWords.length; i++)
     shouldBeTrue("({ " + reservedWords[i] + ": true})."+reservedWords[i]);

var accessorLiteral = "({";
for (var i = 0; i < reservedWords.length; i++) {
    accessorLiteral += "get " + reservedWords[i] + "(){},";
    accessorLiteral += "set " + reservedWords[i] + "(){},"; 
}
accessorLiteral += " parsed: true })";

shouldBeTrue(accessorLiteral + ".parsed");

var successfullyParsed = true;
