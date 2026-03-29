function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

 function test1(array)
{
    var sum = 0;
    for (var value of array)
        sum += value;
    return sum;
}
noInline(test1);

 function test2(array)
{
    var sum = 0;
    for (var value of array.keys())
        sum += value;
    return sum;
}
noInline(test2);

 function test3(array)
{
    var sum = 0;
    for (var [key, value] of array.entries())
        sum += (value + key);
    return sum;
}
noInline(test3);

 function test4(array)
{
    var sum = 0;
    for (var [key, value] of array.values())
        sum += (value + key);
    return sum;
}
noInline(test3);

 var array = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10];
for (var i = 0; i < testLoopCount; ++i) {
    shouldBe(test1(array), 55);
    shouldBe(test2(array), 55);
    shouldBe(test3(array), 110);
    shouldBe(test4(array), 55);
}