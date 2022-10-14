function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test1() {
    return "/assets/omfg".slice(0,8) === '/assets/';
}
noInline(test1);

var count = 0;
for (var i = 0; i < 1e6; ++i) {
    if (test1())
        ++count;
}
shouldBe(count, 1e6);
