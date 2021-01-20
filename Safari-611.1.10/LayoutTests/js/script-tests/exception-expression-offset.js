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
        shouldBeEqualToString("ex.message", message);
    }
}

function func() {}

testException("undefined.a++", 0, 9, 11, "undefined is not an object (evaluating 'undefined.a')");
testException("++undefined.a", 2, 11, 13, "undefined is not an object (evaluating 'undefined.a')");
testException("undefined[0]++", 0, 9, 12, "undefined is not an object (evaluating 'undefined[0]')");
testException("++undefined[1]", 2, 11, 14, "undefined is not an object (evaluating 'undefined[1]')");
testException("undefined.b", 0, 9, 11, "undefined is not an object (evaluating 'undefined.b')");
testException("undefined[0]", 0, 9, 12, "undefined is not an object (evaluating 'undefined[0]')");
testException("undefined.b += 1", 0, 9, 11, "undefined is not an object (evaluating 'undefined.b')");
testException("undefined[0] += 1", 0, 9, 12, "undefined is not an object (evaluating 'undefined[0]')");
testException("undefined()", 0, 9, 11, "undefined is not a function. (In 'undefined()', 'undefined' is undefined)");
testException("new undefined()", 0, 13, 15, "undefined is not a constructor (evaluating 'new undefined()')");
testException("({}).b()", 0, 6, 8, "({}).b is not a function. (In '({}).b()', '({}).b' is undefined)");
testException("new {}.b()", 0, 8, 10, "undefined is not a constructor (evaluating 'new {}.b()')");
testException("1()", 0, 1, 3, "1 is not a function. (In '1()', '1' is 1)");
testException("new 1()", 0, 5, 7, "1 is not a constructor (evaluating 'new 1()')");
testException("throw { message : 'thrown object' }", 0, undefined, 35, "thrown object");
testException("1 in undefined", 0, 5, 14, "undefined is not an Object. (evaluating '1 in undefined')");
testException("1 instanceof undefined", 0, 13, 22, "Right hand side of instanceof is not an object");
testException("for (undefined.b in [1]) {}", 5, 14, 16, "undefined is not an object (evaluating 'undefined.b')");
testException("for (undefined[0] in [1]) {}", 5, 14, 17, "undefined is not an object (evaluating 'undefined[0]')");
testException("undefined.a = 5", 0, 9, 15, "undefined is not an object (evaluating 'undefined.a = 5')");
testException("undefined[0] = 5", 0, 9, 16, "undefined is not an object (evaluating 'undefined[0] = 5')");
testException("({b:undefined}).b.a = 5", 0, 17, 23, "undefined is not an object (evaluating '({b:undefined}).b.a = 5')");
testException("({b:undefined}).b[0] = 5", 0, 17, 24, "undefined is not an object (evaluating '({b:undefined}).b[0] = 5')");
testException("undefined.a += 5", 0, 9, 11, "undefined is not an object (evaluating 'undefined.a')");
testException("undefined[0] += 5", 0, 9, 12, "undefined is not an object (evaluating 'undefined[0]')");
testException("({b:undefined}).b.a += 5", 0, 17, 19, "undefined is not an object (evaluating '({b:undefined}).b.a')");
testException("({b:undefined}).b[0] += 5", 0, 17, 20, "undefined is not an object (evaluating '({b:undefined}).b[0]')");

testException("[].a.b.x", 0, 4, 5, "undefined is not an object (evaluating '[].a.b')");
testException("[]['a']['b'].x", 0, 7, 11, "undefined is not an object (evaluating '[]['a']['b']')");
testException("[].a['b'].x", 0, 4, 8, "undefined is not an object (evaluating '[].a['b']')");
testException("[]['a'].b.x", 0, 7, 8, "undefined is not an object (evaluating '[]['a'].b')");

testException("func(undefined.x)", 5, 14, 15, "undefined is not an object (evaluating 'undefined.x')");
testException("func(null.x)", 5, 9, 10, "null is not an object (evaluating 'null.x')");
testException("func(undefined[0])", 5, 14, 16, "undefined is not an object (evaluating 'undefined[0]')");
testException("func(null[0])", 5, 9, 11, "null is not an object (evaluating 'null[0]')");
