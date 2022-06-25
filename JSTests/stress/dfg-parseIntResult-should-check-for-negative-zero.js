function assertEq(x, y) {
    if (x != y)
        throw "FAILED: expect " + y + ", actual " + x;
}
noInline(assertEq);

function test() {
    assertEq(1.0 / parseInt("-0.0"), "-Infinity");
}
noInline(test);

for (let i = 0; i < 10000; ++i)
    test();
