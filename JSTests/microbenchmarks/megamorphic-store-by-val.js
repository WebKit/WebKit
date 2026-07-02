//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
(function() {
    var array = [];
    var names = ['a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p'];
    for (var i = 0; i < 100; ++i) {
        var o = {};
        o["i" + i] = i;
        o[names[i & 15]] = 42;
        array.push(o);
    }

    for (var i = 0; i < 1000000; ++i) {
        var name = names[i & 15];
        var instance = array[i % array.length];
        instance[name] = i;
        if (instance[name] != i)
            throw "Error: bad result: " + result;
    }
})();
