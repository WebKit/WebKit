// Defining a private field consults [[IsExtensible]] once per field, which for a Proxy runs a
// user-supplied trap. Every tier and every inline cache has to cope with that call-out.
// https://tc39.es/proposal-nonextensible-applies-to-private/

class Base { constructor(object) { return object; } }

class WithFields extends Base {
    #x = 1;
    #y = 2;
    static read(object) { return `${object.#x},${object.#y}`; }
    static hasX(object) { return #x in object; }
    static hasY(object) { return #y in object; }
}

function shouldBe(actual, expected, message) {
    if (actual !== expected)
        throw new Error(`${message}: expected ${expected}, got ${actual}`);
}

function tryDefine(object) {
    try {
        new WithFields(object);
        return "defined";
    } catch (error) {
        if (!(error instanceof TypeError))
            throw error;
        return "TypeError";
    }
}

// All non-callable proxies in a realm share one structure, so a field transition cached on the
// structure alone would skip the trap for every proxy.
{
    let trapCalls = 0;
    for (let i = 0; i < testLoopCount; ++i) {
        const proxy = new Proxy({}, { isExtensible() { ++trapCalls; return true; } });
        shouldBe(tryDefine(proxy), "defined", `extensible proxy at ${i}`);
        shouldBe(WithFields.read(proxy), "1,2", `fields on extensible proxy at ${i}`);
    }
    shouldBe(trapCalls, testLoopCount * 2, "one trap call per field");

    trapCalls = 0;
    const nonExtensible = new Proxy(Object.preventExtensions({}), {
        isExtensible() { ++trapCalls; return false; }
    });
    shouldBe(tryDefine(nonExtensible), "TypeError", "non-extensible proxy");
    shouldBe(trapCalls, 1, "trap calls on non-extensible proxy");
    shouldBe(WithFields.hasX(nonExtensible), false, "#x on non-extensible proxy");
}

// The trap can make the target non-extensible between two fields, which leaves the first one in place.
{
    for (let i = 0; i < testLoopCount; ++i) {
        const target = {};
        let trapCalls = 0;
        const proxy = new Proxy(target, {
            isExtensible() {
                if (++trapCalls === 2)
                    Object.preventExtensions(target);
                return Reflect.isExtensible(target);
            }
        });
        shouldBe(tryDefine(proxy), "TypeError", `target frozen between fields at ${i}`);
        shouldBe(WithFields.hasX(proxy), true, `#x before the target was frozen at ${i}`);
        shouldBe(WithFields.hasY(proxy), false, `#y after the target was frozen at ${i}`);
    }
}

// [[IsExtensible]] is consulted before the field is looked up, so a trap that defines the same
// field re-entrantly makes the outer definition fail instead of defining it twice.
{
    for (let i = 0; i < testLoopCount; ++i) {
        let trapCalls = 0;
        let reentered = false;
        let target;
        target = new Proxy({}, {
            isExtensible() {
                ++trapCalls;
                if (!reentered) {
                    reentered = true;
                    new WithFields(target);
                }
                return true;
            }
        });

        shouldBe(tryDefine(target), "TypeError", `re-entrant duplicate field at ${i}`);
        shouldBe(WithFields.read(target), "1,2", `fields defined inside the trap at ${i}`);
        shouldBe(trapCalls, 3, `trap calls across the re-entrant definition at ${i}`);
    }
}

// A throwing trap wins, and nothing is defined.
{
    for (let i = 0; i < testLoopCount; ++i) {
        const thrower = new Proxy({}, { isExtensible() { throw new RangeError("trap"); } });
        let caught = null;
        try {
            new WithFields(thrower);
        } catch (error) {
            caught = error;
        }
        if (!(caught instanceof RangeError))
            throw new Error(`expected the trap's RangeError, got ${caught}`);
        shouldBe(WithFields.hasX(thrower), false, `#x after throwing trap at ${i}`);
    }
}

// Private accessors on ordinary objects go through the brand, which a cache warmed up on extensible
// objects must still refuse to install on a non-extensible one.
{
    class WithAccessor extends Base {
        get #a() { return "a"; }
        set #a(value) { }
        static read(object) { return object.#a; }
        static has(object) { return #a in object; }
    }

    for (let i = 0; i < testLoopCount; ++i) {
        const extensible = {};
        new WithAccessor(extensible);
        shouldBe(WithAccessor.read(extensible), "a", `accessor on extensible object at ${i}`);

        for (const nonExtensible of [Object.preventExtensions({}), Object.seal({}), Object.freeze({})]) {
            let caught = null;
            try {
                new WithAccessor(nonExtensible);
            } catch (error) {
                caught = error;
            }
            if (!(caught instanceof TypeError))
                throw new Error(`expected TypeError for accessor on non-extensible object, got ${caught}`);
            shouldBe(WithAccessor.has(nonExtensible), false, `accessor on non-extensible object at ${i}`);
        }
    }
}
