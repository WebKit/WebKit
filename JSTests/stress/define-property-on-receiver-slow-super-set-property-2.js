(function() {
    var target = {};
    var proxy = new Proxy(target, {});

    var { method } = { method(i) { super.foo = i; } };
    for (var i = 0; i < testLoopCount; i++)
        method.call(proxy, i);

    if (target.foo !== i - 1)
        throw new Error("Bad value!");
})();
