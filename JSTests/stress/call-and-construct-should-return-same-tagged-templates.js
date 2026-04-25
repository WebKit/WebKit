function shouldBe(actual, expected)
{
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function call(site)
{
    return site;
}

function poly()
{
    return call`Cocoa`;
}

var first = poly();
var second = new poly();

shouldBe(first, second);
