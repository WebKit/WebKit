for (let i = 0; i <= testLoopCount; ++i) {
    let error;
    let returnCalled = false;
    let iterable = {
        next() { return this; },
        done: false,
        get value() {
            throw error = new Error();
        },
        [Symbol.iterator]() { return this; },
        return() {
            returnCalled = true;
        }
    };
    try {
        for (a of iterable) {
            break;
            throw new Error();
        }
    } catch (e) {
        if (e !== error)
            throw e;
    }

    if (returnCalled)
        throw new Error();
}
