(function() {
    var target = {k0: 0, k1: 1, k2: 2, k3: 3, k4: 4, k5: 5, k6: 6, k7: 7, k8: 8, k9: 9};
    var targetKeys = Object.keys(target);
    var proxy = new Proxy(target, {});

    var lastObj = {};
    for (var i = 0; i < 2e5; ++i)
        lastObj = Object.assign({}, proxy);

    if (Object.keys(lastObj).join() !== targetKeys.join())
        throw new Error("Bad assertion!");
})();
