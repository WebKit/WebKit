var createDOMJITGetterObject = $vm.createDOMJITGetterObject;

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${String(actual)}`);
}

function shouldThrow(func, errorMessage) {
    var errorThrown = false;
    var error = null;
    try {
        func();
    } catch (e) {
        errorThrown = true;
        error = e;
    }
    if (!errorThrown)
        throw new Error('not thrown');
    if (String(error) !== errorMessage)
        throw new Error(`bad error: ${String(error)}`);
}

var domjit = createDOMJITGetterObject();
for (var i = 0; i < 1e3; ++i) {
    shouldThrow(() => {
        Reflect.get(domjit, 'customGetter', { customGetter: 42 });
    }, `TypeError: The DOMJITNode.customGetter getter can only be used on instances of DOMJITNode`);
}
