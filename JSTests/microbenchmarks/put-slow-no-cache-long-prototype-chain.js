(function() {
    var base = { set foo(_v) {} };
    for (var i = 0; i < 1e3; i++)
        base = Object.create(base);

    for (var j = 0; j < 3e3; j++)
        base["foo" + j] = j;
})();
