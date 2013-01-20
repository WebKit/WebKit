// Tests the performance of completely polymorphic strict equality.

var array = [];

for (var i = 0; i < 1000; ++i) {
    array.push(i);
    array.push((i%2) == 0);
    array.push("" + i);
    var o = {};
    o["a" + i] = i + 1;
    array.push(o);
}

var numStrictEqual = 0;
for (var i = 0; i < array.length; ++i) {
    for (var j = i + 1; j < array.length; ++j) {
        if (array[i] === array[j])
            numStrictEqual++;
    }
}

if (numStrictEqual != 249500)
    throw "Incorrect result: " + numStrictEqual;



