function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

for (var { x = 0 in [0] } of [0]) {
    shouldBe(x, true);
}

for (var { x = (0 in [0]) } of [0]) {
    shouldBe(x, true);
}

for (var { x = 0 in [0] } in [0]) {
    shouldBe(x, true);
}

for (var { x = (0 in [0]) } in [0]) {
    shouldBe(x, true);
}
