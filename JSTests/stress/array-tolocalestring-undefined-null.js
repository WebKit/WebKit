function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

shouldBe([{toLocaleString(){}}].toLocaleString(), `undefined`);
shouldBe([{toLocaleString(){ return null }}].toLocaleString(), `null`);
