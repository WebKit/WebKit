(function() {
    var source = [0, 1, 2, 3, 4];
    for (var i = 0; i < 1e6; ++i)
        var array = Array.from(source.values());
    if (array[4] !== 4)
        throw new Error("Bad assertion!");
})();
