//@ requireOptions("--useIteratorHelpers=1")

function assert(x) {
    if (!x)
        throw new Error("Bad assertion!");
}

(function() {
    var arr = new Array(20).fill(0);

    for (var i = 0; i < testLoopCount; i++)
        mapped = arr.values().map((_, c) => c).toArray();

    assert(mapped.length === 20);
    assert(mapped.every((x, i) => x === i));
})();
