function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

String.prototype._startsWith = function (find) {
    return this.substring(0,find.length) === find
}

function test1() {
    return "/assets/omfg"._startsWith('/assets/');
}
noInline(test1);

function test2(string) {
    return "/assets/omfg"._startsWith(string);
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
