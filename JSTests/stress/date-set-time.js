var date = new Date();

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function addSet(date, value)
{
    return date.setTime(date.getTime() + value);
}
noInline(addSet);

for (var i = 0; i < 1e4; ++i) {
    var before = date.getTime();
    var result = addSet(date, i);
    shouldBe(result, before + i);
}
shouldBe(addSet(date, 8.64E15 - date.getTime()), 8.64E15);
shouldBe(Number.isNaN(addSet(date, 8.641E15 - date.getTime())), true);
date.setTime(0);
shouldBe(Number.isNaN(addSet(date, NaN)), true);
