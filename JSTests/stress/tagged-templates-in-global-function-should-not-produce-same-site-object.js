function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function call(site)
{
    return site;
}

var expr = "return call`Cocoa`";
var firstFunction = Function(expr);
var secondFunction = Function(expr);
var first = firstFunction();
var second = secondFunction();
shouldBe(first !== second, true);

shouldBe(first, firstFunction());
shouldBe(first, new firstFunction());
shouldBe(second, secondFunction());
shouldBe(second, new secondFunction());
