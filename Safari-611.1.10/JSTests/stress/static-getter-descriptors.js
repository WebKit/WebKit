function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var descriptor = Object.getOwnPropertyDescriptor(RegExp.prototype, 'global');
shouldBe(JSON.stringify(descriptor), '{"enumerable":false,"configurable":true}');

shouldBe(descriptor.enumerable, false);
shouldBe(descriptor.configurable, true);
shouldBe(descriptor.set, undefined);
shouldBe(typeof descriptor.get, 'function');
