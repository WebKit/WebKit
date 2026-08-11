//@ memoryHog!
//@ skip if $addressBits <= 32
//@ runDefault

'use strict';

function target() {
    arguments.length = 0x10000001;
    return Array.from(arguments);
}

try {
    const result = target(1, 2, 3);
    if (result.length !== 0x10000001)
        throw new Error("bad length: " + result.length);
    if (result[0] !== 1 || result[1] !== 2 || result[2] !== 3)
        throw new Error("bad elements: " + result[0] + ", " + result[1] + ", " + result[2]);
} catch (e) {
    if (!(e instanceof RangeError))
        throw e;
}
