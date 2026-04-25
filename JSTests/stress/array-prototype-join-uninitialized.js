function testArray(array, expected) {
    var s = array.join('M');
    if (s !== expected)
        throw("Bad result for array " + array + " expected: \"" + expected + "\" but got: \"" + s + "\"");
}

function testABC(n, resA, resB, resC) {
    testArray(new Array(n), resA);
    testArray(new B(n), resB);
    testArray(new C(n), resC);
}

class B extends Array { }
class C extends B { }


testABC(0, "", "", "");
testABC(1, "", "", "");
testABC(3, "MM", "MM", "MM")

B.prototype[0] = "foo";
testABC(0, "", "", "");
testABC(1, "", "foo", "foo");
testABC(3, "MM", "fooMM", "fooMM");

C.prototype[1] = "bar";
testABC(0, "", "", "");
testABC(1, "", "foo", "foo");
testABC(3, "MM", "fooMM", "fooMbarM");

Array.prototype[1] = "baz";
testABC(0, "", "", "");
testABC(1, "", "foo", "foo");
testABC(3, "MbazM", "fooMbazM", "fooMbarM");
