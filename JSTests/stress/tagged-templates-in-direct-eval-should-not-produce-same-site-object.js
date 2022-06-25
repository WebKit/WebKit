function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function call(site)
{
    return site;
}

var expr = "call`Cocoa`";
var first = eval(expr);
var second = eval(expr);
shouldBe(first !== second, true);
