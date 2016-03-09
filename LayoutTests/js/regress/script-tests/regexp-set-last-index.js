(function() {
    var re = /foo/g;
    var n = 10000000;
    for (var i = 0; i < n; ++i)
        re.lastIndex = 5;
    re.exec("foo bar foo");
    if (re.lastIndex != "foo bar foo".length)
        throw "Error: bad value of lastIndx: " + re.lastIndex;
})();
