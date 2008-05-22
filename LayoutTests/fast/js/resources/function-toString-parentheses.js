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
    serializedString = serializedString.replace(/^\s*|\s*$/g, ''); // Opera adds whitespace
    return serializedString;
}

function compileAndSerializeLeftmostTest(expression)
{
    var f = eval("(function () { " + expression + "; })");
    var serializedString = f.toString();
    serializedString = serializedString.replace(/[ \t\r\n]+/g, " ");
    serializedString = serializedString.replace("function () { ", "");
    serializedString = serializedString.replace("; }", "");
    serializedString = serializedString.replace(/^\s*|\s*$/g, ''); // Opera adds whitespace
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
            : "'(a " + opHigher + " b) " + opLower + " c'"
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
    shouldBe("compileAndSerialize('(" + op + "a) + b')", 
             removesExtraParentheses ?
             "'" + op + "a + b'" :
             "'(" + op + "a) + b'");
    shouldBe("compileAndSerialize('" + op + "(a + b)')", "'" + op + "(a + b)'");
    shouldBe("compileAndSerialize('!" + op + "a')", "'!" + op + "a'");
    shouldBe("compileAndSerialize('!(" + op + "a)')", 
             removesExtraParentheses ? "'!" + op + "a'" : "'!(" + op + "a)'");
}

shouldBe("compileAndSerialize('!a++')", "'!a++'");
shouldBe("compileAndSerialize('!(a++)')", removesExtraParentheses ? "'!a++'" : "'!(a++)'" );
shouldBe("compileAndSerialize('(!a)++')", "'(!a)++'");
shouldBe("compileAndSerialize('!a--')", "'!a--'");
shouldBe("compileAndSerialize('!(a--)')", removesExtraParentheses ? "'!a--'" : "'!(a--)'");
shouldBe("compileAndSerialize('(!a)--')", "'(!a)--'");

shouldBe("compileAndSerialize('(-1)[a]')", "'(-1)[a]'");
shouldBe("compileAndSerialize('(-1)[a] = b')", "'(-1)[a] = b'");
shouldBe("compileAndSerialize('(-1)[a] += b')", "'(-1)[a] += b'");
shouldBe("compileAndSerialize('(-1)[a]++')", "'(-1)[a]++'");
shouldBe("compileAndSerialize('++(-1)[a]')", "'++(-1)[a]'");
shouldBe("compileAndSerialize('(-1)[a]()')", "'(-1)[a]()'");

shouldBe("compileAndSerialize('new (-1)()')", "'new (-1)()'");

shouldBe("compileAndSerialize('(-1).a')", "'(-1).a'");
shouldBe("compileAndSerialize('(-1).a = b')", "'(-1).a = b'");
shouldBe("compileAndSerialize('(-1).a += b')", "'(-1).a += b'");
shouldBe("compileAndSerialize('(-1).a++')", "'(-1).a++'");
shouldBe("compileAndSerialize('++(-1).a')", "'++(-1).a'");
shouldBe("compileAndSerialize('(-1).a()')", "'(-1).a()'");

shouldBe("compileAndSerialize('(- 0)[a]')", "'(- 0)[a]'");
shouldBe("compileAndSerialize('(- 0)[a] = b')", "'(- 0)[a] = b'");
shouldBe("compileAndSerialize('(- 0)[a] += b')", "'(- 0)[a] += b'");
shouldBe("compileAndSerialize('(- 0)[a]++')", "'(- 0)[a]++'");
shouldBe("compileAndSerialize('++(- 0)[a]')", "'++(- 0)[a]'");
shouldBe("compileAndSerialize('(- 0)[a]()')", "'(- 0)[a]()'");

shouldBe("compileAndSerialize('new (- 0)()')", "'new (- 0)()'");

shouldBe("compileAndSerialize('(- 0).a')", "'(- 0).a'");
shouldBe("compileAndSerialize('(- 0).a = b')", "'(- 0).a = b'");
shouldBe("compileAndSerialize('(- 0).a += b')", "'(- 0).a += b'");
shouldBe("compileAndSerialize('(- 0).a++')", "'(- 0).a++'");
shouldBe("compileAndSerialize('++(- 0).a')", "'++(- 0).a'");
shouldBe("compileAndSerialize('(- 0).a()')", "'(- 0).a()'");

shouldBe("compileAndSerialize('(1)[a]')", removesExtraParentheses ? "'1[a]'" : "'(1)[a]'");
shouldBe("compileAndSerialize('(1)[a] = b')", removesExtraParentheses ? "'1[a] = b'" : "'(1)[a] = b'");
shouldBe("compileAndSerialize('(1)[a] += b')", removesExtraParentheses ? "'1[a] += b'" : "'(1)[a] += b'");
shouldBe("compileAndSerialize('(1)[a]++')", removesExtraParentheses ? "'1[a]++'" : "'(1)[a]++'");
shouldBe("compileAndSerialize('++(1)[a]')", removesExtraParentheses ? "'++1[a]'" : "'++(1)[a]'");
shouldBe("compileAndSerialize('(1)[a]()')", removesExtraParentheses ? "'1[a]()'" : "'(1)[a]()'");

shouldBe("compileAndSerialize('new (1)()')", removesExtraParentheses ? "'new 1()'" : "'new (1)()'");

shouldBe("compileAndSerialize('(1).a')", "'(1).a'");
shouldBe("compileAndSerialize('(1).a = b')", "'(1).a = b'");
shouldBe("compileAndSerialize('(1).a += b')", "'(1).a += b'");
shouldBe("compileAndSerialize('(1).a++')", "'(1).a++'");
shouldBe("compileAndSerialize('++(1).a')", "'++(1).a'");
shouldBe("compileAndSerialize('(1).a()')", "'(1).a()'");

for (i = 0; i < assignmentOperators.length; ++i) {
    var op = assignmentOperators[i];
    shouldBe("compileAndSerialize('(-1) " + op + " a')", "'(-1) " + op + " a'");
    shouldBe("compileAndSerialize('(- 0) " + op + " a')", "'(- 0) " + op + " a'");
    shouldBe("compileAndSerialize('1 " + op + " a')", "'1 " + op + " a'");
}

shouldBe("compileAndSerializeLeftmostTest('({ }).x')", "'({ }).x'");
shouldBe("compileAndSerializeLeftmostTest('x = { }')", "'x = { }'");
shouldBe("compileAndSerializeLeftmostTest('(function () { })()')", "'(function () { })()'");
shouldBe("compileAndSerializeLeftmostTest('x = function () { }')", "'x = function () { }'");

shouldBe("compileAndSerializeLeftmostTest('var a')", "'var a'");
shouldBe("compileAndSerializeLeftmostTest('var a = 1')", "'var a = 1'");
shouldBe("compileAndSerializeLeftmostTest('var a, b')", "'var a, b'");
shouldBe("compileAndSerializeLeftmostTest('var a = 1, b = 2')", "'var a = 1, b = 2'");
shouldBe("compileAndSerializeLeftmostTest('var a, b, c')", "'var a, b, c'");
shouldBe("compileAndSerializeLeftmostTest('var a = 1, b = 2, c = 3')", "'var a = 1, b = 2, c = 3'");

shouldBe("compileAndSerializeLeftmostTest('const a = 1')", "'const a = 1'");
shouldBe("compileAndSerializeLeftmostTest('const a = (1, 2)')", "'const a = (1, 2)'");
shouldBe("compileAndSerializeLeftmostTest('const a, b = 1')", "'const a, b = 1'");
shouldBe("compileAndSerializeLeftmostTest('const a = 1, b')", "'const a = 1, b'");
shouldBe("compileAndSerializeLeftmostTest('const a = 1, b = 1')", "'const a = 1, b = 1'");
shouldBe("compileAndSerializeLeftmostTest('const a = (1, 2), b = 1')", "'const a = (1, 2), b = 1'");
shouldBe("compileAndSerializeLeftmostTest('const a = 1, b = (1, 2)')", "'const a = 1, b = (1, 2)'");
shouldBe("compileAndSerializeLeftmostTest('const a = (1, 2), b = (1, 2)')", "'const a = (1, 2), b = (1, 2)'");

var successfullyParsed = true;
