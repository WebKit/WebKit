description(
"Tests that out-of-bounds stores to a Uint8ClampedArray that are aliased to out-of-bounds loads don't crash."
);

function foo(a, i, v)
{
    var result = a[i];
    a[i] = v;
    return result;
}

for (var i = 0; i < 200; ++i) {
    var array = new Uint8ClampedArray(1);
    shouldBe("foo(array, 100000000, 42)", "" + (void 0));
}
