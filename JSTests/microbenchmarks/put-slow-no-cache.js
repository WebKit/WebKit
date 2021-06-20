(function() {
    var base = { set foo(_v) {} };

    for (var j = 0; j < 1e5; j++)
        base["foo" + j] = j;
})();
