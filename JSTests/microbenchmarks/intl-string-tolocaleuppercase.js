function test() {
    let count = 0;
    for (let i = 0; i < 1e5; ++i) {
        if ("Hello World".toLocaleUpperCase() === "HELLO WORLD")
            ++count;
        if ("hello world".toLocaleUpperCase("en") === "HELLO WORLD")
            ++count;
    }
    return count;
}

const result = test();
if (result !== 2e5)
    throw new Error("Bad result: " + result);
