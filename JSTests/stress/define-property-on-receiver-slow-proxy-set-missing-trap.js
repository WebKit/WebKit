(function() {
    var target = {};
    var proxy = new Proxy(target, {});

    for (var i = 0; i < 1e6; i++)
        proxy.foo = i;

    if (target.foo !== i - 1)
        throw new Error("Bad value!");
})();
