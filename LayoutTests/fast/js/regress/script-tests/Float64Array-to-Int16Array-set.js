//@ skip

(function() {
    var result = 0;
    
    for (var i = 0; i < 100000; ++i) {
        var array1 = new Float64Array(15);
        for (var j = 0; j < array1.length; ++j)
            array1[j] = i - j;
        var array2 = new Int16Array(15);
        for (var j = 0; j < 10; ++j)
            array2.set(array1);
        for (var j = 0; j < array2.length; ++j)
            result += array2[j];
    }
    
    if (result != 7243531440)
        throw "Error: bad result: " + result;
})();
