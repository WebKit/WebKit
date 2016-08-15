var target = { __proto__: null };
var handler = {
    get: function ()
    {
        throw new Error(`ng`);
    }
};

var thrown = null;
var hello = 42;
// As a result, global object automatically inherit this proxy.
Object.prototype.__proto__ = {
    __proto__: new Proxy(target, handler)
};

try {
    loadString(`hello = {"hello":"world"};proxy.ok = {}`);
} catch (error) {
    thrown = error;
}
Object.prototype.__proto__ = null;
if (thrown === null)
    throw new Error(`not thrown`);
if (String(thrown) !== `Error: ng`)
    throw new Error(`bad error: ${String(thrown)}`);
