function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test0(object)
{
    for (var key in object = object) {
        if (!object.hasOwnProperty(key))
            return false;
    }
    return true;
}
noInline(test0);

function test1(object)
{
    for (var key in (object, object = object)) {
        if (!object.hasOwnProperty(key))
            return false;
    }
    return true;
}
noInline(test1);

function test2(object)
{
    for (var key in (object, object)) {
        if (!object.hasOwnProperty(key))
            return false;
    }
    return true;
}
noInline(test2);

var object = {
    a: 42,
    b: 43,
    c: 44,
    d: 45,
};
for (var i = 0; i < 1e4; ++i) {
    shouldBe(test0(object), true);
    shouldBe(test1(object), true);
    shouldBe(test2(object), true);
}
