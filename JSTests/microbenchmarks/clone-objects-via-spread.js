function test(object)
{
    return { ...object };
}
noInline(test);

var object = {
    "hello": 1,
    "world": 2,
    "this": 3,
    "is": 4,
    "a": 5,
    "test": 6,
    "for": 7,
    "super": 8,
    "fast": 9,
    "cloning": 10,
    "via": 11,
    "spread": 12,
    "expression": 13,
};

for (var i = 0; i < 1e6; ++i)
    test(object);
