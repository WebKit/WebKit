(function() {
    var root = {};
    var leaf = root;
    for (var i = 0; i < 10; ++i)
        leaf = Object.create(leaf);

    function test(root, leaf) {
        return root.isPrototypeOf(leaf);
    }
    noInline(test);

    for (var i = 0; i < 2e6; ++i) {
        if (!test(root, leaf))
            throw "Error: bad result";
    }
})();
