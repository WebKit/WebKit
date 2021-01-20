function shouldBe(actual, expected)
{
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test(array, val1, val2, val3, val4, val5, val6, val7, val8, val9, val10, val11, val12)
{
    return array.push(val1, val2, val3, val4, val5, val6, val7, val8, val9, val10, val11, val12);
}
noInline(test);

var values = [ 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7, 1.8, 1.9, 2.0, 2.1, 2.2 ];
shouldBe(values.length, 12);
for (var i = 0; i < 1e5; ++i) {
    var array = [];
    shouldBe(test(array, values[0], values[1], values[2], values[3], values[4], values[5], values[6], values[7], values[8], values[9], values[10], values[11]), 12);
    for (var j = 0; j < values.length; ++j)
        shouldBe(array[j], values[j]);
}
