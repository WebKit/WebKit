//@ defaultNoNoLLIntRun if $architecture == "arm"

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}


function tag(site)
{
    return site;
}

{
    for (var i = 0; i < 1e4; ++i)
        eval(`(function () { return tag\`${i}\`; })()`);
}
gc();
