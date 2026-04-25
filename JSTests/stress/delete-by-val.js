function assert(condition) {
    if (!condition)
        throw new Error("assertion failed");
}

function test(i) {
    let foo = {};
    foo["bar" + i] = 1;
    assert(foo["bar" + i] === 1)
    assert(delete foo["bar" + i]);
    assert(!("bar" + i in foo));
}

for (let i = 0; i < testLoopCount; ++i)
    test(i);
