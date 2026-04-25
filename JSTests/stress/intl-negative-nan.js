function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function foo() {
    eval(`function bar() {}`);
    const x = -undefined;
    let s = x.toLocaleString();
    shouldBe(s, "NaN");
    return s;
}
for (let i = 0; i < 1000; i++) {
    foo();
}
