description('This test exercises the source expression offset information that is attached to exception objects for the inspector.');
var ex;

function testException(code, errorStart, errorCaret, errorEnd, message) {
    try {
        debug("");
        debug("Testing '"+code+"'");
        eval(code);
    } catch (e) {
        ex = e;
        shouldBeTrue('ex.expressionBeginOffset == ' + errorStart);
        shouldBeTrue('ex.expressionCaretOffset == ' + errorCaret);
        shouldBeTrue('ex.expressionEndOffset == ' + errorEnd);
        shouldBeTrue('ex.message == "' + message +'"');
    }
}

testException("undefined.a++", 0, 9, 11, "Result of expression 'undefined' [undefined] is not an object.");
testException("++undefined.a", 2, 11, 13, "Result of expression 'undefined' [undefined] is not an object.");
testException("undefined[0]++", 0, 9, 12, "Result of expression 'undefined' [undefined] is not an object.");
testException("++undefined[1]", 2, 11, 14, "Result of expression 'undefined' [undefined] is not an object.");
testException("undefined.b", 0, 9, 11, "Result of expression 'undefined' [undefined] is not an object.");
testException("undefined[0]", 0, 9, 12, "Result of expression 'undefined' [undefined] is not an object.");
testException("undefined.b += 1", 0, 9, 11, "Result of expression 'undefined' [undefined] is not an object.");
testException("undefined[0] += 1", 0, 9, 12, "Result of expression 'undefined' [undefined] is not an object.");
testException("undefined()", 0, 9, 11, "Result of expression 'undefined' [undefined] is not a function.");
testException("new undefined()", 0, 13, 15, "Result of expression 'undefined' [undefined] is not a constructor.");
testException("({}).b()", 0, 6, 8, "Result of expression '({}).b' [undefined] is not a function.");
testException("new {}.b()", 0, 8, 10, "Result of expression '{}.b' [undefined] is not a constructor.");
testException("1()", 0, 1, 3, "Result of expression '1' [1] is not a function.");
testException("new 1()", 0, 5, 7, "Result of expression '1' [1] is not a constructor.");
testException("throw { message : 'thrown object' }", 0, undefined, 35, "thrown object");
testException("1 in undefined", 0, 5, 14, "Result of expression 'undefined' [undefined] is not a valid argument for 'in'.");
testException("1 instanceof undefined", 0, 13, 22, "Result of expression 'undefined' [undefined] is not a valid argument for 'instanceof'.");
testException("for (undefined.b in [1]) {}", 5, 14, 16, "Result of expression 'undefined' [undefined] is not an object.");
testException("for (undefined[0] in [1]) {}", 5, 14, 17, "Result of expression 'undefined' [undefined] is not an object.");
testException("undefined.a = 5", 0, 9, 15, "Result of expression 'undefined' [undefined] is not an object.");
testException("undefined[0] = 5", 0, 9, 16, "Result of expression 'undefined' [undefined] is not an object.");
testException("({b:undefined}).b.a = 5", 0, 17, 23, "Result of expression '({b:undefined}).b' [undefined] is not an object.");
testException("({b:undefined}).b[0] = 5", 0, 17, 24, "Result of expression '({b:undefined}).b' [undefined] is not an object.");
testException("undefined.a += 5", 0, 9, 11, "Result of expression 'undefined' [undefined] is not an object.");
testException("undefined[0] += 5", 0, 9, 12, "Result of expression 'undefined' [undefined] is not an object.");
testException("({b:undefined}).b.a += 5", 0, 17, 19, "Result of expression '({b:undefined}).b' [undefined] is not an object.");
testException("({b:undefined}).b[0] += 5", 0, 17, 20, "Result of expression '({b:undefined}).b' [undefined] is not an object.");

var successfullyParsed = true;
