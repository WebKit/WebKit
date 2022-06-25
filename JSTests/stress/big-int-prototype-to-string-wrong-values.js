function assert(a) {
    if (!a)
        throw new Error("Bad assertion");
}

function assertRangeError(v) {
    let a = 456n;
    try {
        a.toString(v);
        assert(false);
    } catch (e) {
        assert(e instanceof RangeError);
    }
}

assertRangeError(1);
assertRangeError(37);
assertRangeError(37.1);
assertRangeError(37.2);
assertRangeError(0);
assertRangeError(-1);
assertRangeError(1.999999);
assertRangeError(37.00000000000000001);
assertRangeError(NaN);
assertRangeError(null);
assertRangeError(+Infinity);
assertRangeError(-Infinity);
assertRangeError(-0);

