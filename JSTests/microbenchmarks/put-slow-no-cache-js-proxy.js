(function() {
    var global = $vm.createGlobalObject();
    Object.defineProperty(global, 'foo', {
        set(_v) { }
    });
    var proxy = $vm.createGlobalProxy(global);

    for (var j = 0; j < 1e5; j++)
        proxy["foo" + j] = j;
})();
