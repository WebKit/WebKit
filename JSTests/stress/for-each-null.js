function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var caller = null;

function test()
{
    caller = test.caller;
}

function topLevel() {
    [0].forEach(test);
}
topLevel();
shouldBe(caller, null);
