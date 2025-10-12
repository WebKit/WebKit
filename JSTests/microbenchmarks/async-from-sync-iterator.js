function test(syncIterable) {
    return Array.fromAsync(syncIterable);
}
noInline(test);

const syncIterable = {
    [Symbol.iterator]() {
        return {
            next() {
                return { value: undefined, done: true };
            }
        };
    }
};

for (let i = 0; i < 1e5; i++) {
    test(syncIterable);
}
