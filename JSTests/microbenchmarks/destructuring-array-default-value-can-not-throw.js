(function() {
    var arr = [1, 2];
    var c = 0;
    for (var i = 0; i < 1e7; i++) {
        var [a, b] = arr;
        c += a + b;
    }

    if (c !== 3e7)
        throw new Error("Bad value!");
})();
