function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test() { }

var result = test;
for (var i = 0; i < 6; ++i)
    result = result.bind();
shouldBe(result.name, `bound bound bound bound bound bound test`);
