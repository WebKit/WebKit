(function() {
    var trapCalls = 0;
    var proxy = new Proxy({}, {
        defineProperty() {
            trapCalls++;
            return true;
        }
    });

    var { method } = { method() { super.foo = 1; } };
    for (var i = 0; i < 1e6; i++)
        method.call(proxy);

    if (trapCalls !== i)
        throw new Error("Bad value!");
})();
