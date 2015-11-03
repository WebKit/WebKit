description("Regression test for 150745");

// We should be able to ORS exit from an inlined tail callee of a getter.  This test shouldn't crash.

"use strict";

class Test {
    constructor(a, b)
    {
        this.a = a;
        this.b = b;
        this.callCount = 0;
    }

    get sum()
    {
        return this.doSum(1, 2);
    }

    doSum(dummy1, dummy2)
    {
        this.callCount++;

        if (this.callCount == 49000)
            this.dfgCompiled = true;

        if (this.callCount == 199000)
            this.ftlCompiled = true;

        return this.a + this.b;
    }
}

var testObj = new Test(40, 2);

function getSum(o)
{
    return o.sum;
}

for (var i = 0; i < 500000; i++) {
    var result = getSum(testObj);
    if (result != 42)
        testFailed("Expected 42 from \"sum\" getter, got " + result);
}

testPassed("Able to OSR exit from an inlined tail callee of a getter.");
