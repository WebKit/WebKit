var result = (function(){
    var o = {};
    for (var i = 0; i < 100; ++i)
        o["a" + i] = 42;
    var result = [];
    var strings = [];
    for (var i = 0; i < 100; ++i)
        strings.push("a" + i);
    for (var j = 0; j < 1000; ++j) {
        for (var i = 0; i < 100; ++i)
            result.push(o[strings[i]]);
    }
    return result.length;
})();

if (result != 100000)
    throw "Error: bad result: " + result;
