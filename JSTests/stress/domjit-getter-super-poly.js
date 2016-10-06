function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${String(actual)}`);
}

var domjits = [];
for (var i = 0; i < 100; ++i)
    domjits.push(createDOMJITGetterObject());

function access(domjit)
{
    return domjit.customGetter + domjit.customGetter;
}

for (var i = 0; i < 1e2; ++i) {
    for (var j = 0; j < domjits.length; ++j)
        shouldBe(access(domjits[j]), 84);
}
