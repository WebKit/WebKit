//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
(function() {
    var array = [];
    for (var i = 0; i < 1000; ++i) {
        var o = {};
        o["i" + i] = i;
        o.f = 42;
        array.push(o);
    }

    function test(array, index) {
        return test2(array, index, 'f');
    }

    function test2(array, index, name) {
        return array[index][name];
    }
    noInline(test);

    for (var i = 0; i < 1000000; ++i) {
        var result = test(array, i % array.length);
        if (result != 42)
            throw "Error: bad result: " + result;
    }
})();
