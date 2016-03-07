(function() {
    var result;
    for (var i = 0; i < 400000; ++i) {
        result = "foo".replace(/f/, "b");
    }
    if (result != "boo")
        throw "Error: bad result: "+ result;
})();
