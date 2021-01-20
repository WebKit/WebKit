function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var object = {
    toString() { return "Cocoa"; }
};
var charAt = String.prototype.charAt;
function test(object)
{
    shouldBe(charAt.call(object, 0), `C`);
}
noInline(test);

for (var i = 0; i < 1e5; ++i)
    test(object);
