function test2() {
    return new Error("Hey");
}

function test1() {
    return test2();
}
noInline(test1);

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

for (let i = 0; i < testLoopCount; ++i) {
    let error = test1();
    shouldBe(error.line, 2);
    shouldBe(error.column, 21);
}
