function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${String(actual)}`);
}

var domjit1 = createDOMJITGetterObject();
var domjit2 = createDOMJITGetterObject();

function access(domjit)
{
    return domjit.customGetter + domjit.customGetter;
}

for (var i = 0; i < 1e4; ++i)
    shouldBe(access((i & 0x1) ? domjit1 : domjit2), 84);

shouldBe(access({ customGetter: 42 }), 84);
domjit1.test = 44;
shouldBe(access(domjit1), 84);
