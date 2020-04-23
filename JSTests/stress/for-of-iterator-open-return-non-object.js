function vendIterator() {
    return 1;
}
noInline(vendIterator);

function test() {
    let iterable = {
        [Symbol.iterator]() { return vendIterator(); },
        next() { return { done: true }; }
    }
    for (let o of iterable)
        throw new Error();
    throw new Error();
}
noInline(test);

for (let i = 0; i < 1e5; ++i) {
    let error;
    try {
        test();
    } catch (e) {
        error = e;
    }
    if (error != "TypeError: Iterator result interface is not an object.")
        throw new Error();
}
