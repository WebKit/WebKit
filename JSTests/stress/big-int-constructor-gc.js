function assert(expected, value) {
    if (expected !== value)
        throw new Error("Bad assertion. Expected: " + expected + " and value was: " + value);
}

let arr = [];

for (let i = 0; i < testLoopCount; i++) {
    arr[i] = BigInt(i.toString());
}

gc();

for (let i = 0; i < testLoopCount; i++) {
    assert(i.toString(), arr[i].toString());
}

