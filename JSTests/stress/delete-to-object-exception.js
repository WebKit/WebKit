function assert(condition) {
    if (!condition)
        throw new Error("assertion failed");
}

function test(i) {
    let value = null;
    let passed = true;

    try {
        delete value.bar;
        passed = false;
    } catch(e) {}
    try {
        delete value["bar" + i];
        passed = false;
    } catch(e) {}
    if (!passed)
        throw new Error("didn't throw");
}
noInline(test);

for (let i = 0; i < 10000; ++i)
    test();
