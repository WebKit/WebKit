var createDOMJITGetterObject = $vm.createDOMJITGetterObject;

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${String(actual)}`);
}

var domjit = createDOMJITGetterObject();

function access(domjit)
{
    return domjit.customGetter2 + domjit.customGetter2;
}

for (var i = 0; i < 1e4; ++i)
    shouldBe(access(domjit), 84);

shouldBe(access({ customGetter2: 42 }), 84);
domjit.test = 44;
shouldBe(access(domjit), 84);
