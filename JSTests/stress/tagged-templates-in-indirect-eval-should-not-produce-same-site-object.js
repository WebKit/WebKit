function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var indirectEval = eval;
function call(site)
{
    return site;
}

var expr = "call`Cocoa`";
var first = indirectEval(expr);
var second = indirectEval(expr);
shouldBe(first !== second, true);
