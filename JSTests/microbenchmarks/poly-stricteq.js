//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
//@ runNoFTL

// Tests the performance of completely polymorphic strict equality.

var array = [];

for (var i = 0; i < 1000; ++i) {
    array.push((i % 2) == 0);
    array.push(3.14 * i);
    array.push("" + i);
    var o = {};
    o["a" + i] = i + 1;
    array.push(o);
}

var numStrictEqual = 0;

function foo(x, y)
{
    if(x === y)
        numStrictEqual++;
}

function test()
{
    for (var i = 0; i < array.length; ++i) {
        for (var j = i + 1; j < array.length; ++j) {
            foo(array[i], array[j]);
        }
    }

    if (numStrictEqual != 249500)
        throw "Incorrect result: " + numStrictEqual;
}
noInline(test);
test();


