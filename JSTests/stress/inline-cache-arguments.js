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

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

class A extends Function {}
let a = new A("'use strict';");
let b = new A();

function test(object) {
    return object.arguments;
}
noInline(test);

for (var i = 0; i < 1e6; ++i) {
    shouldBe(typeof test(b), "object");
    shouldBe(typeof test({ arguments: { } }), "object");
    shouldBe(typeof test({ arguments: { }, hello: 42 }), "object");
    shouldBe(typeof test({ arguments: { }, world: 42 }), "object");
    shouldBe(typeof test({ arguments: { }, cocoa: 42 }), "object");
    shouldBe(typeof test({ arguments: { }, cappuccino: 42 }), "object");
}

shouldThrow(() => {
    test(a);
}, `TypeError: 'arguments', 'callee', and 'caller' cannot be accessed in this context.`);
