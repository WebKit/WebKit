(function() {
    function tag(array, value) {
        array.tag = value;
        return array;
    }
    noInline(tag);

    var result = 0;
    for (var i = 0; i < 2000000; ++i) {
        var array = tag([i, i + 1], i);
        result += array.tag + array[1];
    }
    if (result !== 4000000000000)
        throw new Error("Bad result: " + result);
})();
