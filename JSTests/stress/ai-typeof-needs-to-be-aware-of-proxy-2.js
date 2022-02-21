function assert(b) {
    if (!b)
        throw new Error;
}

var builtin = $vm.createBuiltin(`(function (a) {
    if (@isProxyObject(a)) {
        if (typeof a === "object")
            return false;
    }
    return true;
})`);

noInline(builtin);

var builtin2 = $vm.createBuiltin(`(function (a) {
    if (@isProxyObject(a)) {
        if (typeof a === "function")
            return true;
    }
    return false;
})`);
noInline(builtin2);

let p = new Proxy(function(){}, {});
for (let i = 0; i < 10000; ++i) {
    assert(builtin(p) === true);
    assert(builtin2(p) === true);
}
