//@ memoryHog!

function testSet() {
    const oomString = '\u1234'.padEnd(0x7fffffff, 'a');
    const set = new Set();

    set.add(1);
    set.add(2);
    set.add(3);
    set.add(4);

    set.delete(1);
    set.delete(2);
    set.delete(3);
    set.delete(4);

    try {
        set.add(oomString, null);
    } catch (e) {
        if (!(e instanceof RangeError))
            throw e;
    }

    if (set.size !== 0)
        throw new Error("wrong size");
}

// This should run in a loop but it takes forever so just run it once.
testSet();
