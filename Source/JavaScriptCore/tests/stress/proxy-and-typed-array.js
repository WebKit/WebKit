var array = [0, 1, 2, 3];
var proxy = new Proxy(array, {
    get: function (target, name, receiver)
    {
        throw new Error(String(name));
    }
});
var thrown = null;
try {
    var typedArray = new Uint8Array(proxy);
} catch (error) {
    thrown = error;
}
if (thrown === null)
    throw new Error(`not thrown`);
if (String(thrown) !== `Error: Symbol(Symbol.iterator)`)
    throw new Error(`bad error: ${String(thrown)}`);
