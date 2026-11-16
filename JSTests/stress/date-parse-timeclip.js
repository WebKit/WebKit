function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

[
    "1970-01-01T00:00:00.000500001Z",
    "1969-12-31T23:59:59.999515625Z",
    "1969-12-31T23:59:59.999015625Z",
].forEach(str => {
    const tv = Date.parse(str);
    shouldBe(Object.is(tv, 0), true);
    shouldBe((new Date(str)).toISOString(), `1970-01-01T00:00:00.000Z`);
});

[
    0.500001,
    -0.484375,
    -0.984375,
].forEach(value => {
    shouldBe(new Date(value).valueOf(), 0);
});
