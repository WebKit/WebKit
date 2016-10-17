function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${String(actual)}`);
}

var complex = createDOMJITGetterComplexObject();
function access(complex)
{
    return complex.customGetter;
}
noInline(access);
for (var i = 0; i < 1e4; ++i) {
    shouldBe(access(complex), 42);
}
