function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

let aggregateError = new AggregateError([0, 1, 2]);

let descriptor = Object.getOwnPropertyDescriptor(aggregateError, "errors");
shouldBe(descriptor.writable, true);
shouldBe(descriptor.enumerable, false);
shouldBe(descriptor.configurable, true);
shouldBe(Array.isArray(descriptor.value), true);
shouldBe(descriptor.value.length, 3);
for (let i = 0; i < descriptor.value.length; ++i)
    shouldBe(descriptor.value[i], i);

shouldBe("errors" in AggregateError.prototype, false);
