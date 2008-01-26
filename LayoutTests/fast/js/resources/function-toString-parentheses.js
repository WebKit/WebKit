description(
"This test checks that parentheses are preserved when significant, and not added where inappropriate. " +
"We need this test because the JavaScriptCore parser removes all parentheses and the serializer then adds them back."
);

function compileAndSerialize(expression)
{
    var f = eval("(function () { return " + expression + "; })");
    var serializedString = f.toString();
    serializedString = serializedString.replace(/[ \t\r\n]+/g, " ");
    serializedString = serializedString.replace("function () { return ", "");
    serializedString = serializedString.replace("; }", "");
    return serializedString;
}

function compileAndSerializeLeftmostTest(expression)
{
    var f = eval("(function () { " + expression + "; })");
    var serializedString = f.toString();
    serializedString = serializedString.replace(/[ \t\r\n]+/g, " ");
    serializedString = serializedString.replace("function () { ", "");
    serializedString = serializedString.replace("; }", "");
    return serializedString;
}

var removesExtraParentheses = compileAndSerialize("(a + b) + c") == "a + b + c";

function testLeftAssociativeSame(opA, opB)
{
    shouldBe("compileAndSerialize('a " + opA + " b " + opB + " c')",
        "'a " + opA + " b " + opB + " c'");
    shouldBe("compileAndSerialize('(a " + opA + " b) " + opB + " c')",
        removesExtraParentheses
            ? "'a " + opA + " b " + opB + " c'"
            : "'(a " + opA + " b) " + opB + " c'"
    );
    shouldBe("compileAndSerialize('a " + opA + " (b " + opB + " c)')",
        "'a " + opA + " (b " + opB + " c)'");
}

function testRightAssociativeSame(opA, opB)
{
    shouldBe("compileAndSerialize('a " + opA + " b " + opB + " c')",
        "'a " + opA + " b " + opB + " c'");
    shouldBe("compileAndSerialize('(a " + opA + " b) " + opB + " c')",
        "'(a " + opA + " b) " + opB + " c'");
    shouldBe("compileAndSerialize('a " + opA + " (b " + opB + " c)')",
        removesExtraParentheses
            ? "'a " + opA + " b " + opB + " c'"
            : "'a " + opA + " (b " + opB + " c)'"
    );
}

function testHigherFirst(opHigher, opLower)
{
    shouldBe("compileAndSerialize('a " + opHigher + " b " + opLower + " c')",
        "'a " + opHigher + " b " + opLower + " c'");
    shouldBe("compileAndSerialize('(a " + opHigher + " b) " + opLower + " c')",
        removesExtraParentheses
            ? "'a " + opHigher + " b " + opLower + " c'"
            : "'a " + opHigher + " (b " + opLower + " c)'"
    );
    shouldBe("compileAndSerialize('a " + opHigher + " (b " + opLower + " c)')",
        "'a " + opHigher + " (b " + opLower + " c)'");
}

function testLowerFirst(opLower, opHigher)
{
    shouldBe("compileAndSerialize('a " + opLower + " b " + opHigher + " c')",
        "'a " + opLower + " b " + opHigher + " c'");
    shouldBe("compileAndSerialize('(a " + opLower + " b) " + opHigher + " c')",
        "'(a " + opLower + " b) " + opHigher + " c'");
    shouldBe("compileAndSerialize('a " + opLower + " (b " + opHigher + " c)')",
        removesExtraParentheses
            ? "'a " + opLower + " b " + opHigher + " c'"
            : "'a " + opLower + " (b " + opHigher + " c)'"
    );
}

var binaryOperators = [
    [ "*", "/", "%" ], [ "+", "-" ],
    [ "<<", ">>", ">>>" ],
    [ "<", ">", "<=", ">=", "instanceof", "in" ],
    [ "==", "!=", "===", "!==" ],
    [ "&" ], [ "^" ], [ "|" ],
    [ "&&" ], [ "||" ]
];

for (i = 0; i < binaryOperators.length; ++i) {
    var ops = binaryOperators[i];
    for (j = 0; j < ops.length; ++j) {
        var op = ops[j];
        testLeftAssociativeSame(op, op);
        if (j != 0)
            testLeftAssociativeSame(ops[0], op);
        if (i < binaryOperators.length - 1) {
            var nextOps = binaryOperators[i + 1];
            if (j == 0)
                for (k = 0; k < nextOps.length; ++k)
                    testHigherFirst(op, nextOps[k]);
            else
                testHigherFirst(op, nextOps[0]);
        }
    }
}

var assignmentOperators = [ "=", "*=", "/=" , "%=", "+=", "-=", "<<=", ">>=", ">>>=", "&=", "^=", "|=" ];

for (i = 0; i < assignmentOperators.length; ++i) {
    var op = assignmentOperators[i];
    testRightAssociativeSame(op, op);
    if (i != 0)
        testRightAssociativeSame("=", op);
    testLowerFirst(op, "+");
    shouldThrow("compileAndSerialize('a + b " + op + " c')");
    shouldBe("compileAndSerialize('(a + b) " + op + " c')",
        "'(a + b) " + op + " c'");
    shouldBe("compileAndSerialize('a + (b " + op + " c)')",
        "'a + (b " + op + " c)'");
}

var prefixOperators = [ "delete", "void", "typeof", "++", "--", "+", "-", "~", "!" ];
var prefixOperatorSpace = [ " ", " ", " ", "", "", " ", " ", "", "" ];

for (i = 0; i < prefixOperators.length; ++i) {
    var op = prefixOperators[i] + prefixOperatorSpace[i];
    shouldBe("compileAndSerialize('" + op + "a + b')", "'" + op + "a + b'");
    shouldBe("compileAndSerialize('(" + op + "a) + b')", "'" + op + "a + b'");
    shouldBe("compileAndSerialize('" + op + "(a + b)')", "'" + op + "(a + b)'");
    shouldBe("compileAndSerialize('!" + op + "a')", "'!" + op + "a'");
    shouldBe("compileAndSerialize('!(" + op + "a)')", "'!" + op + "a'");
}

shouldBe("compileAndSerialize('!a++')", "'!a++'");
shouldBe("compileAndSerialize('!(a++)')", "'!a++'");
shouldBe("compileAndSerialize('(!a)++')", "'(!a)++'");
shouldBe("compileAndSerialize('!a--')", "'!a--'");
shouldBe("compileAndSerialize('!(a--)')", "'!a--'");
shouldBe("compileAndSerialize('(!a)--')", "'(!a)--'");
shouldBe("compileAndSerializeLeftmostTest('({ }).x')", "'({ }).x'");
shouldBe("compileAndSerializeLeftmostTest('x = { }')", "'x = { }'");
shouldBe("compileAndSerializeLeftmostTest('(function () { })()')", "'(function () { })()'");
shouldBe("compileAndSerializeLeftmostTest('x = function () { }')", "'x = function () { }'");

var successfullyParsed = true;
