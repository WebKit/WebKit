function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

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

function *gen()
{
    yield new.target;
}

var g = gen();
shouldBe(g.next().value, undefined);

shouldThrow(() => {
    var g2 = new gen();
}, `TypeError: function is not a constructor (evaluating 'new gen()')`);
