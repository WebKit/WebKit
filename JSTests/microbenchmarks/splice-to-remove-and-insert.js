//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
//@ skip

(function () {
    for (var i = 0; i < 100; ++i) {
        var array = [];
        for (var j = 0; j < 1000; ++j)
            array.push(j);
        while (array.length > 10)
            array.splice(array.length / 2, 5, 1, 2, 3);
    }
}());
