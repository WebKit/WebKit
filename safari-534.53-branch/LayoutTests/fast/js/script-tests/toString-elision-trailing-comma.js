description(
"This test checks that toString() round-trip on a function that has a array with elision does not remove a comma."
);

function f1() {
    return [,];
}

function f2() {
    return [1];
}

function f3() {
    return [1,];
}

// this is the first testcase that proves that trailing
// elision comma is not removed
function f4() {
    return [1,,];
}

function f5() {
    return [1,,,];
}
function f6() {
    return [1,,,4];
}

function f7() {
    return [,2,];
}

function f8() {
    return [,2,,];
}

function f9() {
    return [,2,,,5];
}

function f10() {
    return [,,3,,,];
}

function f11() {
    return [,,3,,,6];
}

function f12() {
    return [,undefined];
}

function f13() {
    return [,undefined,];
}

function f14() {
    return [,undefined,,];
}

function f15() {
    return [,,];
}

function f16() {
    return [,,,];
}

shouldBe("typeof undefined", "'undefined'");

unevalf = function(x) { return '(' + x.toString() + ')'; };

function testToStringAndLength(fn, length, lastElement)
{
    // check that array length is correct
    shouldBe(""+ fn +"().length", "" + length);

    // check that last element is what it is supposed to be
    shouldBe(""+ fn +"()[" + length +"-1]", "" + lastElement);

    // check that toString result evaluates to code that can be evaluated
    // and that toString doesn't remove the trailing elision comma.
    shouldBe("unevalf(eval(unevalf("+fn+")))", "unevalf(" + fn + ")");

    // check that toString()ed functions should retain semantics

    shouldBe("eval(unevalf("+fn+"))().length", ""+length);
    shouldBe("eval(unevalf("+fn+"))()[" + length +"-1]", ""+lastElement);
}


testToStringAndLength("f1", 1);
testToStringAndLength("f2", 1, 1);
testToStringAndLength("f3", 1,1);
testToStringAndLength("f4", 2);
testToStringAndLength("f5", 3);
testToStringAndLength("f6", 4, 4);
testToStringAndLength("f7", 2, 2);
testToStringAndLength("f8", 3);
testToStringAndLength("f9", 5, 5);
testToStringAndLength("f10", 5);
testToStringAndLength("f11", 6, 6);
testToStringAndLength("f12", 2);
testToStringAndLength("f13", 2);
testToStringAndLength("f14", 3);
testToStringAndLength("f15", 2);
testToStringAndLength("f16", 3);


var successfullyParsed = true;
