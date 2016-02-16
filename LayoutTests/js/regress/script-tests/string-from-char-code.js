var result = (function() {
    var result;
    for (var i = 0; i < 10000000; ++i)
        result = String.fromCharCode(32);
    return result
})();

if (result != " ")
    throw "Error: bad result: " + result;

