function shouldBe(actual, expected)
{
    if (actual !== expected)
        throw new Error(`bad value: ${String(actual)}`);
}

function shouldThrow(func, errorMessage)
{
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

shouldBe(JSON.stringify(new Proxy([undefined], {})), `[null]`);
shouldBe(JSON.stringify(new Proxy([function() { }], {})), `[null]`);
shouldBe(JSON.stringify(new Proxy({ value: undefined }, {})), `{}`);

shouldThrow(function () {
    let proxy = new Proxy([], {
        get(theTarget, propName) {
            if (propName === 'length')
                throw new Error('ok');
            return 42;
        }
    });
    JSON.stringify(proxy);
}, `Error: ok`);
