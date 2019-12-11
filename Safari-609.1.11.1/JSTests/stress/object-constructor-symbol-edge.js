function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test(target)
{
    return Object(target);
}
noInline(test);

(function () {
    for (var i = 0; i < 1e4; ++i) {
        var object = test(Symbol("Cocoa"));
        shouldBe(object instanceof Symbol, true);
        shouldBe(String(object.valueOf()), `Symbol(Cocoa)`);
    }
}());
