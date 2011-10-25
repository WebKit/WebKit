description('This test exercises the source expression offset information that is attached to exception objects for the inspector.');
var ex;

function testException(code, errorStart, errorCaret, errorEnd, message) {
    try {
        debug("");
        debug("Testing '"+code+"'");
        eval(code);
    } catch (e) {
        ex = e;
        // begin/caret/end are not presently exposed in a web facing interface, so cannot be directly checked.
        shouldBeTrue('ex.message == "' + message +'"');
    }
}

testException("undefined.a++", 0, 9, 11, "'undefined' is not an object (evaluating 'undefined.a')");
testException("++undefined.a", 2, 11, 13, "'undefined' is not an object (evaluating 'undefined.a')");
testException("undefined[0]++", 0, 9, 12, "'undefined' is not an object (evaluating 'undefined[0]')");
testException("++undefined[1]", 2, 11, 14, "'undefined' is not an object (evaluating 'undefined[1]')");
testException("undefined.b", 0, 9, 11, "'undefined' is not an object (evaluating 'undefined.b')");
testException("undefined[0]", 0, 9, 12, "'undefined' is not an object (evaluating 'undefined[0]')");
testException("undefined.b += 1", 0, 9, 11, "'undefined' is not an object (evaluating 'undefined.b')");
testException("undefined[0] += 1", 0, 9, 12, "'undefined' is not an object (evaluating 'undefined[0]')");
testException("undefined()", 0, 9, 11, "'undefined' is not a function (evaluating 'undefined()')");
testException("new undefined()", 0, 13, 15, "'undefined' is not a constructor (evaluating 'new undefined()')");
testException("({}).b()", 0, 6, 8, "'undefined' is not a function (evaluating '({}).b()')");
testException("new {}.b()", 0, 8, 10, "'undefined' is not a constructor (evaluating 'new {}.b()')");
testException("1()", 0, 1, 3, "'1' is not a function (evaluating '1()')");
testException("new 1()", 0, 5, 7, "'1' is not a constructor (evaluating 'new 1()')");
testException("throw { message : 'thrown object' }", 0, undefined, 35, "thrown object");
testException("1 in undefined", 0, 5, 14, "'undefined' is not a valid argument for 'in' (evaluating '1 in undefined')");
testException("1 instanceof undefined", 0, 13, 22, "'undefined' is not a valid argument for 'instanceof' (evaluating '1 instanceof undefined')");
testException("for (undefined.b in [1]) {}", 5, 14, 16, "'undefined' is not an object (evaluating 'undefined.b')");
testException("for (undefined[0] in [1]) {}", 5, 14, 17, "'undefined' is not an object (evaluating 'undefined[0]')");
testException("undefined.a = 5", 0, 9, 15, "'undefined' is not an object (evaluating 'undefined.a = 5')");
testException("undefined[0] = 5", 0, 9, 16, "'undefined' is not an object (evaluating 'undefined[0] = 5')");
testException("({b:undefined}).b.a = 5", 0, 17, 23, "'undefined' is not an object (evaluating '({b:undefined}).b.a = 5')");
testException("({b:undefined}).b[0] = 5", 0, 17, 24, "'undefined' is not an object (evaluating '({b:undefined}).b[0] = 5')");
testException("undefined.a += 5", 0, 9, 11, "'undefined' is not an object (evaluating 'undefined.a')");
testException("undefined[0] += 5", 0, 9, 12, "'undefined' is not an object (evaluating 'undefined[0]')");
testException("({b:undefined}).b.a += 5", 0, 17, 19, "'undefined' is not an object (evaluating '({b:undefined}).b.a')");
testException("({b:undefined}).b[0] += 5", 0, 17, 20, "'undefined' is not an object (evaluating '({b:undefined}).b[0]')");
