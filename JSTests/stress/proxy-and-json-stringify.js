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

// This test just ensure that proxy.[[Get]]'s throwing works correctly with JSON.stringify.
var proxy = new Proxy([0, 1, 2, 3], {
    get: function (target, name)
    {
        if (name === '2')
            throw new Error('ng');
        return target[name];
    }
});

shouldThrow(() => {
    JSON.stringify(proxy);
}, `Error: ng`);
