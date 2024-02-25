function assert(x) {
    if (!x)
        throw new Error("Bad assertion");
}

var proxyThatReturnsArgs = new Proxy(function() {}, {
    apply(_, __, args) { return args; }
});

(function() {
    var other = createGlobalObject();
    var func = new other.Function("proxy", "return proxy()");

    for (var i = 0; i < 1e6; i++)
        assert(func(proxyThatReturnsArgs) instanceof other.Array);
})();

(function() {
    var other = createGlobalObject();
    other.proxyThatReturnsArgs = proxyThatReturnsArgs;

    for (var i = 0; i < 1e6; i++)
        assert(other.eval("proxyThatReturnsArgs()") instanceof other.Array);
})();

(function() {
    var other = createGlobalObject();

    for (var i = 0; i < 1e6; i++) {
        var mapped = other.Array.prototype.map.call([1], proxyThatReturnsArgs);
        assert(mapped[0] instanceof other.Array);
    }
})();

(function() {
    var other = createGlobalObject();
    other.proxyThatReturnsArgs = proxyThatReturnsArgs;

    for (var i = 0; i < 1e6; i++) {
        var mapped = other.eval("[1].map(proxyThatReturnsArgs)");
        assert(mapped[0] instanceof other.Array);
    }
})();
