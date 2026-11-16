// InitializeInstanceElements consults [[IsExtensible]] once before installing all of a class's private
// methods and accessors, and PrivateFieldAdd consults it once per field.
// https://tc39.es/proposal-nonextensible-applies-to-private/

function shouldBe(actual, expected, message) {
    if (actual !== expected)
        throw new Error(`${message}: expected ${expected}, got ${actual}`);
}

class Base { constructor(object) { return object; } }

class WithMethodsAndFields extends Base {
    #m1() { }
    #m2() { }
    get #a() { return 0; }
    set #a(value) { }
    #f = 1;
    #g = 2;

    static state(object) {
        return [#m1 in object, #m2 in object, #a in object, #f in object, #g in object].map(Number).join("");
    }
}

class WithMethodsOnly extends Base {
    #m1() { }
    #m2() { }
    get #a() { return 0; }

    static state(object) {
        return [#m1 in object, #m2 in object, #a in object].map(Number).join("");
    }
}

class WithFieldsOnly extends Base {
    #f = 1;
    #g = 2;

    static state(object) {
        return [#f in object, #g in object].map(Number).join("");
    }
}

class WithoutPrivateElements extends Base {
    x = 1;
}

function observe(Class) {
    const log = [];
    let proxy;
    proxy = new Proxy({}, {
        isExtensible() {
            log.push(Class.state(proxy));
            return true;
        }
    });
    new Class(proxy);
    log.push(Class.state(proxy));
    return log.join(",");
}

for (let i = 0; i < testLoopCount; ++i) {
    // All methods and accessors appear at once, before any field.
    shouldBe(observe(WithMethodsAndFields), "00000,11100,11110,11111", `methods and fields at ${i}`);
    shouldBe(observe(WithMethodsOnly), "000,111", `methods only at ${i}`);
    shouldBe(observe(WithFieldsOnly), "00,10,11", `fields only at ${i}`);

    let trapCalls = 0;
    new WithoutPrivateElements(new Proxy({}, { isExtensible() { ++trapCalls; return true; } }));
    shouldBe(trapCalls, 0, `no private elements at ${i}`);
}

// A non-extensible object gets none of the methods, and the check runs only once.
for (let i = 0; i < testLoopCount; ++i) {
    let trapCalls = 0;
    const proxy = new Proxy(Object.preventExtensions({}), {
        isExtensible() {
            ++trapCalls;
            return false;
        }
    });
    let caught = null;
    try {
        new WithMethodsAndFields(proxy);
    } catch (error) {
        caught = error;
    }
    shouldBe(caught instanceof TypeError, true, `TypeError at ${i}`);
    shouldBe(trapCalls, 1, `trap calls on non-extensible object at ${i}`);
    shouldBe(WithMethodsAndFields.state(proxy), "00000", `nothing installed at ${i}`);
}

// Static private methods are installed before any static element runs, so a static block that makes
// the constructor non-extensible cannot make them fail.
{
    class StaticMethods {
        static #m() { return "m"; }
        static get #a() { return "a"; }
        static {
            Object.freeze(this);
        }
        static call() { return this.#m() + this.#a; }
    }
    shouldBe(Object.isExtensible(StaticMethods), false, "frozen constructor");
    shouldBe(StaticMethods.call(), "ma", "static private methods on frozen constructor");
}
