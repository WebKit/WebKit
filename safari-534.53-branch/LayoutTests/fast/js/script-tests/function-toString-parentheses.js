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

function testKeepParentheses(expression)
{
  shouldBe("compileAndSerialize('" + expression + "')",
           "'" + expression + "'");
}

function testOptionalParentheses(expression)
{
  stripped_expression = removesExtraParentheses
        ? expression.replace(/\(/g, '').replace(/\)/g, '')
        : expression;
  shouldBe("compileAndSerialize('" + expression + "')",
           "'" + stripped_expression + "'");
}

function testLeftAssociativeSame(opA, opB)
{
    testKeepParentheses("a " + opA + " b " + opB + " c");
    testOptionalParentheses("(a " + opA + " b) " + opB + " c");
    testKeepParentheses("a " + opA + " (b " + opB + " c)");
}

function testRightAssociativeSame(opA, opB)
{
    testKeepParentheses("a " + opA + " b " + opB + " c");
    testKeepParentheses("(a " + opA + " b) " + opB + " c");
    testOptionalParentheses("a " + opA + " (b " + opB + " c)");
}

function testHigherFirst(opHigher, opLower)
{
    testKeepParentheses("a " + opHigher + " b " + opLower + " c");
    testOptionalParentheses("(a " + opHigher + " b) " + opLower + " c");
    testKeepParentheses("a " + opHigher + " (b " + opLower + " c)");
}

function testLowerFirst(opLower, opHigher)
{
    testKeepParentheses("a " + opLower + " b " + opHigher + " c");
    testKeepParentheses("(a " + opLower + " b) " + opHigher + " c");
    testOptionalParentheses("a " + opLower + " (b " + opHigher + " c)");
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
    testKeepParentheses("(a + b) " + op + " c");
    testKeepParentheses("a + (b " + op + " c)");
}

var prefixOperators = [ "delete", "void", "typeof", "++", "--", "+", "-", "~", "!" ];
var prefixOperatorSpace = [ " ", " ", " ", "", "", " ", " ", "", "" ];

for (i = 0; i < prefixOperators.length; ++i) {
    var op = prefixOperators[i] + prefixOperatorSpace[i];
    testKeepParentheses("" + op + "a + b");
    testOptionalParentheses("(" + op + "a) + b");
    testKeepParentheses("" + op + "(a + b)");
    testKeepParentheses("!" + op + "a");
    testOptionalParentheses("!(" + op + "a)");
}


testKeepParentheses("!a++");
testOptionalParentheses("!(a++)");
testKeepParentheses("(!a)++");

testKeepParentheses("!a--");
testOptionalParentheses("!(a--)");
testKeepParentheses("(!a)--");

testKeepParentheses("(-1)[a]");
testKeepParentheses("(-1)[a] = b");
testKeepParentheses("(-1)[a] += b");
testKeepParentheses("(-1)[a]++");
testKeepParentheses("++(-1)[a]");
testKeepParentheses("(-1)[a]()");

testKeepParentheses("new (-1)()");

testKeepParentheses("(-1).a");
testKeepParentheses("(-1).a = b");
testKeepParentheses("(-1).a += b");
testKeepParentheses("(-1).a++");
testKeepParentheses("++(-1).a");
testKeepParentheses("(-1).a()");

testKeepParentheses("(- 0)[a]");
testKeepParentheses("(- 0)[a] = b");
testKeepParentheses("(- 0)[a] += b");
testKeepParentheses("(- 0)[a]++");
testKeepParentheses("++(- 0)[a]");
testKeepParentheses("(- 0)[a]()");

testKeepParentheses("new (- 0)()");

testKeepParentheses("(- 0).a");
testKeepParentheses("(- 0).a = b");
testKeepParentheses("(- 0).a += b");
testKeepParentheses("(- 0).a++");
testKeepParentheses("++(- 0).a");
testKeepParentheses("(- 0).a()");

testOptionalParentheses("(1)[a]");
testOptionalParentheses("(1)[a] = b");
testOptionalParentheses("(1)[a] += b");
testOptionalParentheses("(1)[a]++");
testOptionalParentheses("++(1)[a]");

shouldBe("compileAndSerialize('(1)[a]()')",
    removesExtraParentheses ? "'1[a]()'" : "'(1)[a]()'");

shouldBe("compileAndSerialize('new (1)()')",
    removesExtraParentheses ? "'new 1()'" : "'new (1)()'");

testKeepParentheses("(1).a");
testKeepParentheses("(1).a = b");
testKeepParentheses("(1).a += b");
testKeepParentheses("(1).a++");
testKeepParentheses("++(1).a");
testKeepParentheses("(1).a()");

for (i = 0; i < assignmentOperators.length; ++i) {
    var op = assignmentOperators[i];
    testKeepParentheses("(-1) " + op + " a");
    testKeepParentheses("(- 0) " + op + " a");
    testKeepParentheses("1 " + op + " a");
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

shouldBe("compileAndSerialize('(function () { new (a.b()).c })')", "'(function () { new (a.b()).c })'");

var successfullyParsed = true;
