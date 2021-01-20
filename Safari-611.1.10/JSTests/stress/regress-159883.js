// Regression test for 159883.  This test should not crash or throw an exception.

d = new Date(-0x80000000, 42);
if (d.toString() != "Invalid Date")
    throw "Expected \"Invalid Date\", but got :\"" + d + "\"";
