//@ requireOptions("--useIteratorHelpers=1")

(function() {
    var source = [0, 1, 2, 3, 4];
    for (var i = 0; i < 1e6; ++i)
        var has4 = source.values().some(x => x === 4);
    if (!has4)
        throw new Error("Bad assertion!");
})();
