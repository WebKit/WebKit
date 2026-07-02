function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var table = new WebAssembly.Table({
    element: "funcref",
    initial: 20
});

table.grow(5)
for (var i = 0; i < 25; ++i)
    shouldBe(table.get(i), null);
