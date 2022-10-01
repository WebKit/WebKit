function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function _startsWith(str, find) {
    return str.substring(0,find.length) === find
}

function test1() {
    return _startsWith("/assets/omfg", '/assets/');
}
noInline(test1);

function test2(string) {
    return _startsWith("/assets/omfg", string);
}
noInline(test2);

var count = 0;
for (var i = 0; i < 5e5; ++i) {
    if (test1())
        ++count;
    if (test2('/assets/'))
        ++count;
}
shouldBe(count, 1e6);
