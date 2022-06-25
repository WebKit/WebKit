function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var object = {
    toString() { return "Cocoa"; }
};
var codePointAt = String.prototype.codePointAt;
function test(object)
{
    shouldBe(codePointAt.call(object, 0), 67);
}
noInline(test);

for (var i = 0; i < 1e5; ++i)
    test(object);
