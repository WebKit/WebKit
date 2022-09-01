function shouldThrow(func, errorMessage) {
    var errorThrown = false;
    var error = null;
    try {
        func();
    } catch (e) {
        errorThrown = true;
        error = e;
    }
    if (!errorThrown)
        throw new Error('not thrown');
    if (String(error) !== errorMessage)
        throw new Error(`bad error: ${String(error)}`);
}

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test(regexp) {
    regexp.lastIndex = 42;
    shouldBe(regexp.lastIndex, 42);
    regexp.lastIndex = "hey";
    shouldBe(regexp.lastIndex, "hey");
}
noInline(test);

var regexp = /test/;
for (var i = 0; i < 1e4; ++i) {
    test(regexp);
    test({ lastIndex: 42 });
}

function test2(regexp) {
    regexp.lastIndex = 42;
    shouldBe(regexp.lastIndex, "hey");
    regexp.lastIndex = "ok";
    shouldBe(regexp.lastIndex, "hey");
}
noInline(test2);

var regexp2 = /test2/;
regexp2.lastIndex = "hey";
Object.freeze(regexp2);
for (var i = 0; i < 1e4; ++i)
    test2(regexp2);
