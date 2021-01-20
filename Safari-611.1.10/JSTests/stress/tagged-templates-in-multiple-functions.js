function shouldBe(actual, expected)
{
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function call(site)
{
    return site;
}

function a()
{
    return call`Cocoa`;
}

function b()
{
    return call`Cocoa`;
}

function c()
{
    return [ call`Cocoa`, call`Cocoa` ];
}

shouldBe(a() !== b(), true);
shouldBe(a() === a(), true);
shouldBe(b() === b(), true);
var result = c();
shouldBe(c()[0] === result[0], true);
shouldBe(c()[1] === result[1], true);
shouldBe(result[0] !== result[1], true);
