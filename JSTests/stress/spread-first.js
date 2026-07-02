function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}
noInline(shouldBe);

function test(object, value)
{
    return { ...object, value };
}
noInline(test);

function test2(object, object2)
{
    return { ...object, ...object2 };
}
noInline(test2);

function test3(object, key, value)
{
    return { ...object, [key]: value };
}
noInline(test3);

shouldBe(JSON.stringify(test({ a: 42, b: 43, c: 44 }, 45)), `{"a":42,"b":43,"c":44,"value":45}`);
shouldBe(JSON.stringify(test({ a: 42, b: 43, value: 42, c: 44 }, 45)), `{"a":42,"b":43,"value":45,"c":44}`);
shouldBe(JSON.stringify(test2({ a: 42, b: 43, value: 42, c: 44 }, { d: 44 })), `{"a":42,"b":43,"value":42,"c":44,"d":44}`);
shouldBe(JSON.stringify(test2({ a: 42, b: 43, value: 42, c: 44 }, { c: 45 })), `{"a":42,"b":43,"value":42,"c":45}`);
shouldBe(JSON.stringify(test3({ a: 42, b: 43, value: 42, c: 44 }, 'd', 44)), `{"a":42,"b":43,"value":42,"c":44,"d":44}`);
shouldBe(JSON.stringify(test3({ a: 42, b: 43, value: 42, c: 44 }, 'c', 45)), `{"a":42,"b":43,"value":42,"c":45}`);
