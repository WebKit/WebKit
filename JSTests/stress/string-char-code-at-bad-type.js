function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var object = {
    toString() { return "Cocoa"; }
};
var charCodeAt = String.prototype.charCodeAt;
function test(object)
{
    shouldBe(charCodeAt.call(object, 0), 67);
}
noInline(test);

for (var i = 0; i < 1e5; ++i)
    test(object);
