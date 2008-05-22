description(
"This test checks that toString() round-trip on a function that has prefix, postfix and typeof operators applied to group expression will not remove the grouping. Also checks that evaluation of such a expression produces run-time exception"
);

function postfix_should_preserve_parens(x, y, z) {
    (x, y)++;
    return y;
}

function prefix_should_preserve_parens(x, y, z) {
    ++(x, y);
    return x;

}

function both_should_preserve_parens(x, y, z) {
    ++(x, y)--;
    return x;

}

function postfix_should_preserve_parens_multi(x, y, z) {
    (((x, y)))--;
    return x;
}

function prefix_should_preserve_parens_multi(x, y, z) {
    --(((x, y)));
    return x;
}

function both_should_preserve_parens_multi(x, y, z) {
    ++(((x, y)))--;
    return x;
}

function postfix_should_preserve_parens_multi1(x, y, z) {
    (((x)), y)--;
    return x;
}

function prefix_should_preserve_parens_multi1(x, y, z) {
    --(((x)), y);
    return x;
}

function prefix_should_preserve_parens_multi2(x, y, z) {
    var z = 0;
    --(((x), y), z);
    return x;
}

function postfix_should_preserve_parens_multi2(x, y, z) {
    var z = 0;
    (((x), y) ,z)++;
    return x;
}

// if these return a variable (such as y) instead of
// the result of typeof, this means that the parenthesis
// got lost somewhere.
function typeof_should_preserve_parens(x, y, z) {
    return typeof (x, y);
}

function typeof_should_preserve_parens1(x, y, z) {
    return typeof ((x, y));
}

function typeof_should_preserve_parens2(x, y, z) {
    var z = 33;
    return typeof (z, (x, y));
}

function typeof_should_preserve_parens_multi(x, y, z) {
    var z = 33;
    return typeof ((z,(((x, y)))));
}

unevalf = function(x) { return '(' + x.toString() + ')'; };

function testToString(fn) {
    // check that toString result evaluates to code that can be evaluated
    // this doesn't actually reveal the bug that this test is testing
    shouldBe("unevalf(eval(unevalf("+fn+")))", "unevalf(" + fn + ")");

    // check that grouping operator is still there (this test reveals the bug
    // but will create possible false negative if toString output changes in
    // the future)
    shouldBeTrue("/.*\\(+x\\)*, y\\)/.test(unevalf("+fn+"))");

}

function testToStringAndRTFailure(fn)
{
    testToString(fn);

    // check that function call produces run-time exception
    shouldThrow(""+fn+ "(1, 2, 3);");

    // check that function call produces run-time exception after eval(unevalf)
    shouldThrow("eval(unevalf("+fn+ "))(1, 2, 3);");
}

function testToStringAndReturn(fn, p1, p2, retval)
{

    testToString(fn);

    // check that function call produces correct result
    shouldBe("" + fn + "(" + p1 + ", " + p2 +");", retval);

    // check that function call produces correct result after eval(unevalf)
    shouldBe("eval(unevalf("+fn+ "))" + "(" + p1 + ", " + p2 +");", retval);
}


testToStringAndRTFailure("prefix_should_preserve_parens");
testToStringAndRTFailure("postfix_should_preserve_parens");
testToStringAndRTFailure("both_should_preserve_parens");
testToStringAndRTFailure("prefix_should_preserve_parens_multi");
testToStringAndRTFailure("postfix_should_preserve_parens_multi");
testToStringAndRTFailure("prefix_should_preserve_parens_multi1");
testToStringAndRTFailure("postfix_should_preserve_parens_multi1");
testToStringAndRTFailure("prefix_should_preserve_parens_multi2");
testToStringAndRTFailure("postfix_should_preserve_parens_multi2");

testToStringAndReturn("typeof_should_preserve_parens", "'a'", 1, "'number'");
testToStringAndReturn("typeof_should_preserve_parens1", "'a'", 1, "'number'");
testToStringAndReturn("typeof_should_preserve_parens2", "'a'", 1, "'number'");
testToStringAndReturn("typeof_should_preserve_parens_multi", "'a'", 1, "'number'");


var successfullyParsed = true;
