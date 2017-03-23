function shouldBe(actual, expected)
{
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var array = [];

for (var i = 0; i < 100; ++i)
    array.push(1024 * 1024 * 1024 * 1024 + i);
for (var i = 0; i < 100; ++i)
    array.push(-(1024 * 1024 * 1024 * 1024 + i));

array.push(2251799813685248);
array.push(0.5);

function num()
{
    return 42;
}
noInline(num);

for (var i = 0; i < 1e4; ++i) {
    for (var index = 0; index < 100; ++index) {
        array[index] = (array[index] + 1) + num();
    }
}

