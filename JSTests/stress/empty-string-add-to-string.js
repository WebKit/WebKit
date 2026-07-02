function test1(a) { return a + ''; }
function test2(a) { return '' + a; }

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var object = { [Symbol.toPrimitive](hint) { shouldBe(hint, "default"); return 42; }  };
for (var i = 0; i < 1e4; ++i) {
    shouldBe(test1(i), i.toString());
    shouldBe(test2(i), i.toString());
    shouldBe(test1(object), `42`);
    shouldBe(test2(object), `42`);
}
