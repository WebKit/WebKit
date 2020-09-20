function assert(a, message) {
    if (!a)
        throw new Error(message);
}

function assertThrowRangeError(a, b, message) {
    try {
        let n = a << b;
        assert(false, message + ": Should throw RangeError, but executed without exception");
    } catch (e) {
        assert(e instanceof Error, message + ": expected Error , got: " + e);
    }
}

let a = 1n << 64n;
assertThrowRangeError(1n, a, "Left shift by " + a);

a = 1n << 30n;
assertThrowRangeError(1n, a, "Left shift by " + a);

