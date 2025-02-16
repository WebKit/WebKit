for (let i = 0; i < testLoopCount; ++i) {
    let returnCalled = false;
    let iterable = {
        done: 0,
        next() {
            return {
                done: this.done++,
                get value() { if (isFinalTier()) ForceOSR();  }
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
