(function() {
    var result;
    for (var i = 0; i < 400000; ++i) {
        result = "foo".replace(/f/g, 42);
    }
    if (result != "42oo")
        throw "Error: bad result: "+ result;
})();
