function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${String(actual)}`);
}

var domjit = createDOMJITNodeObject();
function access(domjit)
{
    return domjit.customGetter + domjit.customGetter;
}

for (var i = 0; i < 1e4; ++i)
    shouldBe(access(domjit), 84);

shouldBe(access({ customGetter: 42 }), 84);
domjit.test = 44;
shouldBe(access(domjit), 84);
