function test(value) {
    return Iterator.from(value);
}
noInline(test);

let result;
for (let i = 0; i < 1e6; i++) {
    const iter = {
        next() {
            return { value: 0, done: false };
        }
    };
    result = test(iter);
    if (!(result instanceof Iterator)) {
        throw new Error("bad result");
    }
}

