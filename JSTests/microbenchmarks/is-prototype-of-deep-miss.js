(function() {
    var leaf = {};
    for (var i = 0; i < 10; ++i)
        leaf = Object.create(leaf);
    var probe = { unrelated: true };

    function test(probe, leaf) {
        return probe.isPrototypeOf(leaf);
    }
    noInline(test);

    for (var i = 0; i < 2e6; ++i) {
        if (test(probe, leaf))
            throw "Error: bad result";
    }
})();
