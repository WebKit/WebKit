function assert(a) {
    if (!a)
        throw new Error("Bad assertion");
}

function assertThrowRangeError(input) {
    try {
        let number = 3n;
        number.toString(input);
        assert(false);
    } catch (e) {
        assert(e instanceof RangeError);
    }
}

assertThrowRangeError(1e100);
assertThrowRangeError(-1e101);

