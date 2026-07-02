function shouldBe(actual, expected)
{
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function tag(site)
{
    return site;
}

var site1 = eval("0, tag`Cocoa`");
var site2 = eval("1, tag`Cappuccino`");
var site3 = eval("2, tag`Cocoa`");

shouldBe(site1 !== site3, true);
shouldBe(site1 !== site2, true);
