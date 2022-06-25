function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var keys = Object.keys(/Cocoa/);

shouldBe(JSON.stringify(keys.sort()), '[]');  // non enumerable
