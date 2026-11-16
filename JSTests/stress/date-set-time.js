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

// setTime has to drop the cached broken-down date, including from its JIT fast path, which stores
// the new time value inline.
function setThenRead(date, value)
{
    date.setTime(value);
    return `${date.getFullYear()}/${date.getMonth()}/${date.getDate()}/${date.getHours()}/${date.getUTCHours()}`;
}
noInline(setThenRead);

var values = [0, 1e12, -1e12, 5e11, 8.64e15, -8.64e15];
var subject = new Date(0);
for (var i = 0; i < testLoopCount; ++i) {
    var value = values[i % values.length];
    var reference = new Date(value);
    shouldBe(setThenRead(subject, value),
        `${reference.getFullYear()}/${reference.getMonth()}/${reference.getDate()}/${reference.getHours()}/${reference.getUTCHours()}`);
}
