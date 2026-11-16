description("Regression test for 150663");

// We should be able to tail call a native function from a JS callee of C++

"use strict";

class Test {
    constructor(a, b)
    {
        this.a = a;
        this.b = b;
    }

    get sum()
    {
        return Number(this.a + this.b);
    }
}

var testObj = new Test(40, 2);

for (var i = 0; i < 100000; i++) {
    var result = testObj.sum;
    if (result != 42)
        testFailed("Expected 42 from \"sum\" getter, got " + result);
}

testPassed("Able to tail call a native function from a JS callee of C++ code");
