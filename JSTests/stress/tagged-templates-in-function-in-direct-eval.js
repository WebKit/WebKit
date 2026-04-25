function shouldBe(actual, expected)
{
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function call(site)
{
    return site;
}

function test()
{
    return eval("(function ok() { return call`Cocoa`; })()");
}

var first = test();
var second = test();
shouldBe(first !== second, true);
