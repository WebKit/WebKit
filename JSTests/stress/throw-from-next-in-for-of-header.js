for (let i = 0; i <= testLoopCount; ++i) {
    let error;
    let returnCalled;
    let iterable = {
        next() {
            throw error = new Error();
        },
        [Symbol.iterator]() { return this; },
        return() {
            returnCalled = true;
        }
    };
    noInline(iterable.next);
    try {
        for (_ of iterable)
            throw new Error();
    } catch (e) {
        if (e !== error)
            throw e;
    }

    if (returnCalled)
        throw new Error();
}
