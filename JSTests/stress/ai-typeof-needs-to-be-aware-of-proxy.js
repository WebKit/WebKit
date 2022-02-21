function assert(b) {
    if (!b)
        throw new Error;
}

var builtin = $vm.createBuiltin(`(function (a) {
    if (@isProxyObject(a))
        return typeof a;
})`);

noInline(builtin);

let p = new Proxy(function(){}, {});
for (let i = 0; i < 10000; ++i)
    assert(builtin(p) === "function");
