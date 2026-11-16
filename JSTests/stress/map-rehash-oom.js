//@ memoryHog!

function testMap() {
    const oomString = '\u1234'.padEnd(0x7fffffff, 'a');
    const map = new Map();

    map.set(1, null);
    map.set(2, null);
    map.set(3, null);
    map.set(4, null);

    map.delete(1);
    map.delete(2);
    map.delete(3);
    map.delete(4);

    try {
        map.set(oomString, null);
    } catch (e) {
        if (!(e instanceof RangeError))
            throw e;
    }

    if (map.size !== 0)
        throw new Error("wrong size");
}

// This should run in a loop but it takes forever so just run it once.
testMap();
