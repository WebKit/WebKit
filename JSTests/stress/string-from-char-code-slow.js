var result = (function() {
    var result;
    for (var i = 0; i < testLoopCount; ++i)
        result = String.fromCharCode(1000);
    return result
})();

if (result != "Ϩ")
    throw "Error: bad result: " + result;

