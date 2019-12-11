function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

shouldBe((function test() { }).toString(), `function test() { }`);
shouldBe((() => { }).toString(), `() => { }`);
shouldBe((function* test() { }).toString(), `function* test() { }`);
shouldBe((async function* test() { }).toString(), `async function* test() { }`);
shouldBe((async function test() { }).toString(), `async function test() { }`);
shouldBe((async () => { }).toString(), `async () => { }`);
