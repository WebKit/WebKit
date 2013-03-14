var result = (function(){
    var o = {};
    for (var i = 0; i < 100; ++i)
        o["a" + i] = 42;
    var result = [];
    for (var i = 0; i < 100000; ++i)
        result.push(o["a" + i]);
    return result.length;
})();

if (result != 100000)
    throw "Error: bad result: " + result;
