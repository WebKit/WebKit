//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
(function() {
    var array = [];
    for (var i = 0; i < 100; ++i) {
        var o = {};
        o["i" + i] = i;
        o.f = 42;
        array.push(o);
    }

    for (var i = 0; i < 1000000; ++i) {
        var instance = array[i % array.length];
        instance.f = i;
        if (instance.f != i)
            throw "Error: bad result: " + result;
    }
})();
