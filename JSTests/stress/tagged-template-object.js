function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function tag(site)
{
    return site;
}

{
    function a() {
        return tag`Hello`;
    }

    function b() {
        return tag`Hello`;
    }

    shouldBe(a() === b(), false);
    gc();
    var tagA = a();
    gc();
    shouldBe(tagA === b(), false);
}
