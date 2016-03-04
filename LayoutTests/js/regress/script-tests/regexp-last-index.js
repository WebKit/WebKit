(function() {
    var re = /foo/g;
    re.exec("bar foo bar");
    var result = 0;
    var n = 10000000;
    for (var i = 0; i < n; ++i)
        result += re.lastIndex;
    if (result != 7 * n)
        throw "Error: bad result: " + result;
})();
