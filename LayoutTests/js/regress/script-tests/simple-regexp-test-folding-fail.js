(function() {
    for (var i = 0; i < 1000000; ++i)
        /foo/.test("bar");
})();

