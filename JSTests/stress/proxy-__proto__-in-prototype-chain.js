let called = false;
let p = new Proxy({ }, {
    set(obj, prop, value) {
        called = prop === "__proto__";
    }
});
let o = {__proto__: p};
o.__proto__ = null;

if (!called)
    throw new Error;

called = false;
Reflect.set(o, "__proto__", null, {});
if (!called)
    throw new Error;
