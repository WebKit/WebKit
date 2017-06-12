function shouldBe(actual, expected)
{
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function indexOf(array, value, index)
{
    return array.indexOf(value, index);
}
noInline(indexOf);

var object = {
    valueOf()
    {
        return 0;
    }
}
var array = [0,1,2,3,4,5,6,7,8,9];
for (var i = 0; i < 1e5; ++i)
    shouldBe(indexOf(array, 9, object), 9);
