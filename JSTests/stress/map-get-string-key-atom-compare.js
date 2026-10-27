function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ' expected: ' + expected);
}

function get(map, key) {
    return map.get(key);
}
noInline(get);

function has(map, key) {
    return map.has(key);
}
noInline(has);

var map = new Map();
var keys = [];
for (var i = 0; i < 64; ++i) {
    var key = 'key' + i;
    keys.push(key);
    map.set(key, i);
}
map.set('isCompleted', 'a');
map.set('tags', 'b');

var object = { isCompleted: 0, tags: 0 };
var propertyNames = Object.keys(object);

for (var i = 0; i < testLoopCount; ++i) {
    var index = i % 64;
    shouldBe(get(map, keys[index]), index);
    shouldBe(get(map, 'key' + index), index);
    shouldBe(get(map, ['key', index].join('')), index);
    shouldBe(get(map, propertyNames[0]), 'a');
    shouldBe(get(map, propertyNames[1]), 'b');
    shouldBe(get(map, 'isCompleted'), 'a');
    shouldBe(get(map, 'missing' + index), undefined);
    shouldBe(get(map, 'tag'), undefined);
    shouldBe(has(map, 'key' + index), true);
    shouldBe(has(map, 'kez' + index), false);
}
