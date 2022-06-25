function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

try {
    (function () {
        throw new Error("ok");
    }());
} catch (error) {
    for (let line of error.stack.split('\n'))
        shouldBe(line.includes('@'), true);
}

try {
    eval(`
        throw new Error("ok");
    `);
} catch (error) {
    for (let line of error.stack.split('\n'))
        shouldBe(line.includes('@'), true);
}

try {
    eval(`
        (function () {
            throw new Error("ok");
        }());
    `);
} catch (error) {
    for (let line of error.stack.split('\n'))
        shouldBe(line.includes('@'), true);
}
