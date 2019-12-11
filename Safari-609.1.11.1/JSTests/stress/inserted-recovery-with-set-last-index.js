function shouldBe(actual, expected)
{
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function foo(p) {
    var o = /Hello/;
    if (p) {
        var res = /World/;
        res.lastIndex = o;
        return res;
    }
}

noInline(foo);

var array = new Array(1000);
for (var i = 0; i < 400000; ++i) {
    var o = foo(i & 0x1);
    if (i & 0x1) {
        shouldBe(o instanceof RegExp, true);
        shouldBe(o.toString(), "/World/");
        shouldBe(o.lastIndex.toString(), "/Hello/");
    }
    array[i % array.length] = o;
}

