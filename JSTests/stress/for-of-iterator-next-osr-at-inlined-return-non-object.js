let shouldVendNull = false;
function vendNext() {
    if (shouldVendNull)
        return 1;
    return { done: true };
}
noInline(vendNext);

// Pass shouldVendNull as param so we don't OSR when it becomes true.
function test(shouldVendNull) {
    let iterable = {
        [Symbol.iterator]() { return this; },
        next() { return vendNext(); }
    }
    for (let o of iterable)
        throw new Error();
    if (shouldVendNull)
        throw new Error();
}
noInline(test);

for (let i = 0; i < 1e5; ++i)
    test();

shouldVendNull = true;
let error;
try {
    test(true);
} catch (e) {
    error = e;
}

if (error != "TypeError: Iterator result interface is not an object.")
    throw error;
