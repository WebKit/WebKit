//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
//@ skip

for (var i = 0; i < 100; ++i) {
    var array = [];
    for (var j = 0; j < 1000; ++j)
        array.push(j);
    while (array.length)
        array.splice(array.length / 2, 1);
}

