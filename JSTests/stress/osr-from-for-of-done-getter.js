for (let i = 0; i < 1e5; ++i) {
    let returnCalled = false;
    let iterable = {
        next() {
            return {
                get done() {
                    if (isFinalTier())
                        ForceOSR();
                    return true;
                }
            }
        },

        [Symbol.iterator]() { return this; },
        return() { returnCalled = true; },
    };
    try {
        for (_ of iterable) { }
    } catch {
    }
    if (returnCalled)
        throw new Error();
}
