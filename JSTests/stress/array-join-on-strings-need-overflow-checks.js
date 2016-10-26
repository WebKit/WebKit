function assert(x, y) {
    if (x != y)
        throw(" Expect: " + y + ", actual: " + x);
}

s1 = "";
for (var k = 0; k < 2000; ++k)
    s1 += "z";
var expectedLength = 2000;
assert(s1.length, 2000);

s2 = 'x';
expectedLength = 1;
assert(s2.length, expectedLength);

for (var i = 0; i < 22; ++i) {
    expectedLength += expectedLength;
    s2 += s2;
    assert(s2.length, expectedLength);
}

var caughtException;
try {
    expectedLength = ((s1.length - 1) * s2.length) + 1;
    result = Array.prototype.join.apply(s1, [s2]);
    assert(result.length, expectedLength);
} catch (e) {
    caughtException = e;
}

if (!caughtException)
    throw("Array.prototype.join should have thrown an exception when string length overflows");
assert(caughtException, "Error: Out of memory");
