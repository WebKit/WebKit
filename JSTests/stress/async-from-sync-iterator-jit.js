function shouldBe(a, b) {
    if (a !== b)
        throw new Error(`Expected ${b} but got ${a}`);
}

async function test(syncIterable) {
    // This will create AsyncFromSyncIterator internally
    for await (const value of syncIterable) {
        return value;
    }
}
noInline(test);

function getSyncIterable() {
    let count = 0;
    return {
        [Symbol.iterator]() {
            return {
                next() {
                    if (count < 5) {
                        return { value: count++, done: false };
                    }
                    return { value: undefined, done: true };
                }
            };
        }
    };
}

let promises = [];
for (let i = 0; i < testLoopCount; i++) {
    const syncIterable = getSyncIterable();
    promises.push(test(syncIterable));
}

Promise.all(promises).then(results => {
    for (let result of results) {
        shouldBe(result, 0);
    }
});

drainMicrotasks();

