// Installing a private brand consults [[IsExtensible]], which for a Proxy runs a user-supplied trap.
// Every tier and every inline cache has to cope with that call-out.

class Base { constructor(object) { return object; } }

class WithBrand extends Base {
    #method() { return "method"; }
    static has(object) { return #method in object; }
}

function shouldBe(actual, expected, message) {
    if (actual !== expected)
        throw new Error(`${message}: expected ${expected}, got ${actual}`);
}

function tryInstall(object) {
    try {
        new WithBrand(object);
        return "installed";
    } catch (error) {
        if (!(error instanceof TypeError))
            throw error;
        return "TypeError";
    }
}

// A cached brand transition must not skip the trap: all non-callable proxies in a realm share one
// structure, so a cache keyed on the structure alone would answer for every proxy.
{
    for (let i = 0; i < testLoopCount; ++i)
        shouldBe(tryInstall(new Proxy({}, { isExtensible() { return true; } })), "installed", "extensible proxy");

    let trapCalls = 0;
    const nonExtensible = new Proxy(Object.preventExtensions({}), {
        isExtensible() { ++trapCalls; return false; }
    });

    shouldBe(tryInstall(nonExtensible), "TypeError", "non-extensible proxy");
    if (!trapCalls)
        throw new Error("isExtensible trap was never called");
    shouldBe(WithBrand.has(nonExtensible), false, "brand on non-extensible proxy");
}

// The trap can write to unrelated objects, so nothing may be cached across the installation.
{
    function readAcrossInstall(array, object) {
        const before = array[0];
        new WithBrand(object);
        return `${before}/${array[0]}`;
    }

    for (let i = 0; i < testLoopCount; ++i) {
        const array = [1];
        const proxy = new Proxy({}, { isExtensible() { array[0] = 999; return true; } });
        shouldBe(readAcrossInstall(array, proxy), "1/999", `load across brand install at ${i}`);
    }
}

// The trap can reallocate a butterfly, so a cached butterfly pointer would be freed memory.
{
    function readAcrossInstall(array, object) {
        const before = array[0];
        new WithBrand(object);
        return `${before}/${array[0]}/${array.length}`;
    }

    for (let i = 0; i < testLoopCount; ++i) {
        const array = [1.5];
        const proxy = new Proxy({}, {
            isExtensible() {
                for (let j = 0; j < 64; ++j)
                    array.push(j + 0.5);
                array[0] = 7.5;
                return true;
            }
        });
        shouldBe(readAcrossInstall(array, proxy), "1.5/7.5/65", `butterfly across brand install at ${i}`);
    }
}

// The trap can transition the object being branded, which invalidates any structure read before it.
{
    class WithFields extends Base {
        #f0 = 0; #f1 = 1; #f2 = 2; #f3 = 3; #f4 = 4;
        #f5 = 5; #f6 = 6; #f7 = 7; #f8 = 8; #f9 = 9;
        #f10 = 10; #f11 = 11; #f12 = 12; #f13 = 13; #f14 = 14;
        #f15 = 15; #f16 = 16; #f17 = 17; #f18 = 18; #f19 = 19;

        static read(object) {
            return [object.#f0, object.#f9, object.#f19].join(",");
        }
    }

    let stamped = false;
    let target;
    target = new Proxy({}, {
        isExtensible() {
            if (!stamped) {
                stamped = true;
                new WithFields(target);
            }
            return true;
        }
    });

    new WithBrand(target);

    shouldBe(WithBrand.has(target), true, "brand after reentrant transition");
    shouldBe(WithFields.read(target), "0,9,19", "fields stamped inside the trap");
}

// [[IsExtensible]] is consulted before the brand is looked up, so a trap that installs the same
// brand re-entrantly makes the outer installation fail instead of branding the object twice.
{
    let trapCalls = 0;
    let reentered = false;
    let target;
    target = new Proxy({}, {
        isExtensible() {
            ++trapCalls;
            if (!reentered) {
                reentered = true;
                new WithBrand(target);
            }
            return true;
        }
    });

    shouldBe(tryInstall(target), "TypeError", "re-entrant duplicate brand install");
    shouldBe(WithBrand.has(target), true, "brand installed inside the trap");
    shouldBe(trapCalls, 2, "trap calls across the re-entrant install");

    shouldBe(tryInstall(target), "TypeError", "brand already installed");
    shouldBe(trapCalls, 3, "the trap runs before the brand is looked up");
}

// A throwing trap wins, and nothing is installed.
{
    const thrower = new Proxy({}, { isExtensible() { throw new RangeError("trap"); } });
    let caught = null;
    try {
        new WithBrand(thrower);
    } catch (error) {
        caught = error;
    }
    if (!(caught instanceof RangeError))
        throw new Error(`expected the trap's RangeError, got ${caught}`);
    shouldBe(WithBrand.has(thrower), false, "brand after throwing trap");
}
