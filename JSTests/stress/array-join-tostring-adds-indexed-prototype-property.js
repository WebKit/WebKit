function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ' expected ' + expected);
}

// An element's toString adds an indexed property to Array.prototype. Holes visited
// after that point must forward to the prototype instead of being treated as empty.
{
    let calls = 0;
    const obj = {
        toString() {
            calls++;
            Array.prototype[2] = "P";
            return "X";
        }
    };
    const arr = [, obj, , ];
    arr.length = 3;
    shouldBe(arr.join(","), ",X,P");
    shouldBe(calls, 1);
    delete Array.prototype[2];
}

// Multiple holes after the mutation point.
{
    const obj = {
        toString() {
            Array.prototype[1] = "P1";
            Array.prototype[2] = "P2";
            return "X";
        }
    };
    const arr = [obj, , , ];
    arr.length = 3;
    shouldBe(arr.join(","), "X,P1,P2");
    delete Array.prototype[1];
    delete Array.prototype[2];
}

// The added prototype property is an accessor; it must be invoked exactly once.
{
    let getterCalls = 0;
    const obj = {
        toString() {
            Object.defineProperty(Array.prototype, 1, {
                get() {
                    getterCalls++;
                    return "G";
                },
                configurable: true
            });
            return "X";
        }
    };
    const arr = [obj, , ];
    arr.length = 2;
    shouldBe(arr.join(","), "X,G");
    shouldBe(getterCalls, 1);
    delete Array.prototype[1];
}

// Holes visited before the mutation must still read as empty.
{
    const obj = {
        toString() {
            Array.prototype[0] = "P0";
            Array.prototype[2] = "P2";
            return "X";
        }
    };
    const arr = [, obj, , ];
    arr.length = 3;
    shouldBe(arr.join(","), ",X,P2");
    delete Array.prototype[0];
    delete Array.prototype[2];
}
