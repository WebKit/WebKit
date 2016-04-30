(function() {
    for (var j = 0; j < 10; ++j) {
        (function () {
            var regExp = /foo/;
            for (var i = 0; i < 100000; ++i)
                regExp.test("bar");
        })();
    }
})();
