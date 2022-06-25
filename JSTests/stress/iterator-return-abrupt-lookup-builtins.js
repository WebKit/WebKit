function shouldThrow(func, expectedError) {
    let actualError;
    try {
        func();
    } catch (error) {
        actualError = error;
    }
    if (actualError !== expectedError)
        throw new Error(`bad error: ${actualError}`);
}

const iter = {
    [Symbol.iterator]() { return this; },
    next() { return { value: [], done: false }; },
    get return() { throw 'return'; },
};

Map.prototype.set = () => { throw 'set'; };
Set.prototype.add = () => { throw 'add'; };
WeakMap.prototype.set = () => { throw 'set'; };
WeakSet.prototype.add = () => { throw 'add'; };

for (let i = 0; i < 1e4; ++i) {
    shouldThrow(() => new Map(iter), 'set');
    shouldThrow(() => new Set(iter), 'add');
    shouldThrow(() => new WeakMap(iter), 'set');
    shouldThrow(() => new WeakSet(iter), 'add');
}
