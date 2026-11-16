// This should not crash.

try {
    var object = { };
    let global = $vm.createGlobalObject(object);
    let o = $vm.createGlobalProxy(global);
    object.__proto__ = o;

    for (let q in o) { }
} catch { }
