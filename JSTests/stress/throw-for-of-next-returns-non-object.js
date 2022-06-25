for (let i = 0; i < 1e5; ++i) {
    let returnCalled = false;
    let iterable = {
        next() { return 1; },
        [Symbol.iterator]() { return this; },
        return() { returnCalled = true; },
    };
    try {
        for (_ of iterable) { }
    } catch (e) {
        if (e != "TypeError: Iterator result interface is not an object.")
            throw e;
    }
    if (returnCalled)
        throw new Error();
}
