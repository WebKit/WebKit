var result = (function(a) {
    a[0] = 32;

    var result;
    for (var i = 0; i < 1000000; ++i)
        result = String.fromCharCode(a[0]);
    return result
})(new Float64Array(1));

if (result != " ")
    throw "Error: bad result: " + result;

