// A JSGlobalProxy forwards [[IsExtensible]] to its global object, so making the global object
// non-extensible through any proxy blocks private elements on every proxy of it.

class Base { constructor(object) { return object; } }

class WithField extends Base {
    #x = 1;
    static has(object) { return #x in object; }
}

class WithMethod extends Base {
    #m() { return "m"; }
    static has(object) { return #m in object; }
}

function shouldBe(actual, expected, message) {
    if (actual !== expected)
        throw new Error(`${message}: expected ${expected}, got ${actual}`);
}

function tryAdd(Class, object) {
    try {
        new Class(object);
        return "added";
    } catch (error) {
        if (!(error instanceof TypeError))
            throw error;
        return "TypeError";
    }
}

for (let i = 0; i < testLoopCount; ++i) {
    shouldBe(tryAdd(WithField, {}), "added", `field on ordinary object at ${i}`);
    shouldBe(tryAdd(WithMethod, {}), "added", `method on ordinary object at ${i}`);
}

for (let i = 0; i < 20; ++i) {
    const global = $vm.createGlobalObject();
    const first = $vm.createGlobalProxy(global);
    const second = $vm.createGlobalProxy(global);

    shouldBe(tryAdd(WithField, first), "added", `field on extensible global proxy at ${i}`);
    shouldBe(tryAdd(WithMethod, first), "added", `method on extensible global proxy at ${i}`);

    Object.preventExtensions(first);
    shouldBe(Object.isExtensible(second), false, `second proxy after preventExtensions at ${i}`);

    shouldBe(tryAdd(WithField, second), "TypeError", `field on non-extensible global proxy at ${i}`);
    shouldBe(tryAdd(WithMethod, second), "TypeError", `method on non-extensible global proxy at ${i}`);
    shouldBe(WithField.has(second), false, `#x on non-extensible global proxy at ${i}`);
    shouldBe(WithMethod.has(second), false, `#m on non-extensible global proxy at ${i}`);
}

Object.preventExtensions(globalThis);
shouldBe(tryAdd(WithField, globalThis), "TypeError", "field on non-extensible globalThis");
shouldBe(tryAdd(WithMethod, globalThis), "TypeError", "method on non-extensible globalThis");
