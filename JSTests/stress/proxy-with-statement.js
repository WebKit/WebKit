(function () {
    var target = {};
    var handler = {
        get: function ()
        {
            throw new Error('ng');
        }
    };
    var array = {
        hello: 42
    };
    var proxy = new Proxy(target, handler);
    array[Symbol.unscopables] = proxy;
    var thrown = null;
    try {
        with (array) {
            hello;
        }
    } catch (error) {
        thrown = error;
    }
    if (thrown === null)
        throw new Error(`not thrown`);
    if (String(thrown) !== `Error: ng`)
        throw new Error(`bad error: ${String(thrown)}`);
}());
