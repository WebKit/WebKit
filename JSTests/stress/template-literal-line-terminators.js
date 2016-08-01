
function test(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: " + actual);
}

function testEval(script, expected) {
    test(eval(script), expected);
}

function testEvalLineNumber(script, expected, lineNum) {
    testEval(script, expected);

    var error = null;
    var actualLine;
    try {
        eval(script + ';throw new Error("line");');
    } catch (error) {
        actualLine = error.line;
    }
    test(actualLine, lineNum);
}

test(`Hello`, "Hello");
test(`Hello World`, "Hello World");
test(`
`, "\n");
test(`Hello
World`, "Hello\nWorld");

testEvalLineNumber("`Hello World`", "Hello World", 1);

testEvalLineNumber("`Hello\rWorld`", "Hello\nWorld", 2);
testEvalLineNumber("`Hello\nWorld`", "Hello\nWorld", 2);

testEvalLineNumber("`Hello\r\rWorld`", "Hello\n\nWorld", 3);
testEvalLineNumber("`Hello\r\nWorld`", "Hello\nWorld", 2);
testEvalLineNumber("`Hello\n\nWorld`", "Hello\n\nWorld", 3);
testEvalLineNumber("`Hello\n\rWorld`", "Hello\n\nWorld", 2);

testEvalLineNumber("`Hello\n\r\nWorld`", "Hello\n\nWorld", 3);
testEvalLineNumber("`Hello\r\n\rWorld`", "Hello\n\nWorld", 3);
testEvalLineNumber("`Hello\n\n\nWorld`", "Hello\n\n\nWorld", 4);

testEvalLineNumber("`Hello\n\r\n\rWorld`", "Hello\n\n\nWorld", 3);
testEvalLineNumber("`Hello\n\r\n\nWorld`", "Hello\n\n\nWorld", 4);
testEvalLineNumber("`Hello\r\n\n\nWorld`", "Hello\n\n\nWorld", 4);

testEvalLineNumber("`Hello\\\n\r\rWorld`", "Hello\n\nWorld", 3);
testEvalLineNumber("`Hello\\\r\n\n\nWorld`", "Hello\n\nWorld", 4);
testEvalLineNumber("`Hello\\\n\r\n\nWorld`", "Hello\n\nWorld", 4);
testEvalLineNumber("`Hello\\\n\r\r\nWorld`", "Hello\n\nWorld", 3);

testEvalLineNumber("`\u2028`", "\u2028", 2);
testEvalLineNumber("`\u2029`", "\u2029", 2);
testEvalLineNumber("`\\u2028`", "\u2028", 1);
testEvalLineNumber("`\\u2029`", "\u2029", 1);
