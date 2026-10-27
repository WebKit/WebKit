(function() {
    var target = {};
    var handler = {};

    for (var i = 0; i < 1e6; ++i)
        Proxy.revocable(target, handler);
})();
