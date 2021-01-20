description(
"Tests that UInt32ToNumber and OSR exit are aware of copy propagation and correctly recover both versions of a variable that was subject to a UInt32ToNumber cast."
);

function foo(b) {
    var a = b | 0;
    var x, y;
    x = a;
    y = a >>> 0;
    return [x, y];
}

for (var i = 0; i < 100; ++i)
    shouldBe("foo(-1)", "[-1, 4294967295]");

