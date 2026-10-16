(function() {
    var symbol = Symbol("tag");

    function tag(array, value) {
        array[symbol] = value;
        return array;
    }

    var result = 0;
    for (var i = 0; i < 2000000; ++i) {
        var array = tag([], i);
        result += array[symbol] + array.length;
    }
    if (result !== 1999999000000)
        throw new Error("Bad result: " + result);
})();
