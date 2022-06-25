function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test(date)
{
    return date.getTime() + date.getTime();
}
noInline(test);

function test2()
{
    var date = new Date();
    date.setTime(20);
    var first = date.getTime();
    date.setTime(0);
    var second = date.getTime();
    return first + second;
}
noInline(test2);

function test3(date)
{
    return date.getTime() + date.getFullYear();
}
noInline(test3);

var date = new Date();
var result = date.getTime() + date.getTime();
var result2 = (new Date(20)).getTime() + (new Date(0)).getTime();
var result3 = date.getTime() + date.getFullYear();
for (var i = 0; i < 1e6; ++i) {
    shouldBe(test(date), result);
    shouldBe(test2(date), result2);
    shouldBe(test3(date), result3);
}
