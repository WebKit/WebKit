function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test1() {
    return "/assets/omfg".startsWith('/assets/');
}
noInline(test1);

function test2(string) {
    return "/assets/omfg".startsWith(string);
}
noInline(test2);

var count = 0;
for (var i = 0; i < 1e5; ++i) {
    if (test1())
        ++count;
    if (test2('/assets/'))
        ++count;
}
shouldBe(count, 2e5);
