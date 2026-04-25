function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${String(actual)}`);
}

var arrow = () => /Cocoa/;
shouldBe(arrow.toString(), `() => /Cocoa/`);
