//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
(function() {
    var array = [];
    var array2 = [];
    for (var i = 0; i < 100; ++i) {
        var o = {};
        o["i" + i] = i;
        array.push(o);
        var o = {};
        o["i" + i] = i;
        array2.push(o);
    }

    function test(object, i) {
        object.f = i;
    }
    noInline(test);

    for (var i = 0; i < 1000000; ++i) {
        var instance = array[i % array.length];
        test(instance, i);
        if (instance.f !== i)
            throw "Error: bad result: " + result;
    }

    Object.prototype[0] = 43;
    Object.defineProperty(Object.prototype, 'f', {
        get() { return 44; }
    });
    for (var i = 0; i < 1000000; ++i) {
        var instance = array[i % array.length];
        test(instance, i);
        if (instance.f !== i)
            throw "Error: bad result: " + result;
    }

    for (var i = 0; i < 100; ++i) {
        var instance = array2[i % array.length];
        test(instance, i);
        if (instance.f !== 44)
            throw "Error: bad result: " + result;
    }

})();
