function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test1(start, end) {
    return "/assets/omfg".slice(start, end) === '/assets/';
}
noInline(test1);

var count = 0;
for (var i = 0; i < 1e6; ++i) {
    if (test1(0, 8))
        ++count;
}
shouldBe(count, 1e6);
