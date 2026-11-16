function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function makeMap(count) {
    var map = new Map;
    for (var i = 0; i < count; ++i)
        map.set(i, i * 10);
    return map;
}

function keys(map) {
    var result = [];
    map.forEach(function (value, key) { result.push(key + ':' + value); });
    return result.join(',');
}
noInline(keys);

function deleteAhead(map) {
    var result = [];
    map.forEach(function (value, key) {
        result.push(key);
        if (key % 2 == 0)
            map.delete(key + 1);
    });
    return result.join(',');
}
noInline(deleteAhead);

function deleteAllAhead(map) {
    var result = [];
    map.forEach(function (value, key) {
        result.push(key);
        if (key == 2) {
            for (var i = 3; i < 10; ++i)
                map.delete(i);
        }
    });
    return result.join(',');
}
noInline(deleteAllAhead);

function addDuring(map) {
    var result = [];
    var added = 0;
    map.forEach(function (value, key) {
        result.push(key);
        if (added < 40)
            map.set('x' + added++, added);
    });
    return result.length;
}
noInline(addDuring);

function clearDuring(map) {
    var result = [];
    map.forEach(function (value, key) {
        result.push(key);
        if (key == 3)
            map.clear();
    });
    return result.join(',');
}
noInline(clearDuring);

function clearAndReadd(map) {
    var result = [];
    map.forEach(function (value, key) {
        result.push(key);
        if (key == 3) {
            map.clear();
            map.set('a', 1);
            map.set('b', 2);
        }
    });
    return result.join(',');
}
noInline(clearAndReadd);

for (var i = 0; i < testLoopCount; ++i) {
    shouldBe(keys(new Map), '');
    var map = makeMap(5);
    map.delete(0);
    map.delete(4);
    shouldBe(keys(map), '1:10,2:20,3:30');
    map = new Map([[1, 1]]);
    map.delete(1);
    shouldBe(keys(map), '');
    shouldBe(deleteAhead(makeMap(10)), '0,2,4,6,8');
    shouldBe(deleteAllAhead(makeMap(10)), '0,1,2');
    shouldBe(addDuring(makeMap(3)), 43);
    shouldBe(clearDuring(makeMap(10)), '0,1,2,3');
    shouldBe(clearAndReadd(makeMap(10)), '0,1,2,3,a,b');
}
