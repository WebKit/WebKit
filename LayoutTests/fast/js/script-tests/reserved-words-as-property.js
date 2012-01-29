description("Tests to ensure that we can use ES reserved words as property names.");

var reservedWords = ["true", "false", "null", "break", "case", "catch", "continue", "debugger", "default", "delete", "do", "else", "finally", "for",
                     "function", "if", "in", "instanceof", "new", "return", "switch", "this", "throw", "try", "typeof", "var", "void", "while", "with",
                     "class", "const", "enum", "export", "extends", "import", "super"];

var strictReservedWords = [
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


function testWordEvalAndFunction(str, strShouldThrow) {
    if (strShouldThrow) {
        shouldThrow(str);
        shouldThrow("(function(){"+str+"}); true");
    } else {
        shouldBeTrue(str);
        shouldBeTrue("(function(){"+str+"}); true");
    }
}

function testWord(word, strictPrefix, expectedResult) {
    testWordEvalAndFunction(strictPrefix + "var " + word + "; true", expectedResult);
    testWordEvalAndFunction(strictPrefix + "var " + word + " = 42; " + word + " === 42", expectedResult);
    testWordEvalAndFunction(strictPrefix + "function g(" + word + "){ " + strictPrefix + " }; true", expectedResult);
    testWordEvalAndFunction(strictPrefix + "/" + word + "/.test(function g(" + word + "){ " + strictPrefix + " })", expectedResult);
    testWordEvalAndFunction(strictPrefix + "try{}catch(" + word + "){}; true", expectedResult);
    testWordEvalAndFunction(strictPrefix + "function " + word + "(){ " + strictPrefix + " }; true", expectedResult);
    // These should be allowed for all words, even reserved ones.
    testWordEvalAndFunction(strictPrefix + "({ \"" + word + "\": 42 }." + word + " === 42)", false);
    testWordEvalAndFunction(strictPrefix + "({ " + word + ": 42 }." + word + " === 42)", false);
    testWordEvalAndFunction(strictPrefix + "({ get " + word + "(){}, set " + word + "(){}, parsedOkay: 42 }.parsedOkay === 42)", false);
}

function testWordStrictAndNonStrict(word, condition) {
    testWord(word, '', condition == "keyword");
    testWord(word, '"use strict";', condition != "identifier");
}

for (var i = 0; i < reservedWords.length; i++)
    testWordStrictAndNonStrict(reservedWords[i], "keyword");
for (var i = 0; i < strictReservedWords.length; i++)
    testWordStrictAndNonStrict(strictReservedWords[i], "strict");
for (var i = 0; i < unreservedWords.length; i++)
    testWordStrictAndNonStrict(unreservedWords[i], "identifier");

// test access via window.
var yield = 42;
shouldBeTrue("window.yield === 42");
