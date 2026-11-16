function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function makeSet(count) {
    var set = new Set;
    for (var i = 0; i < count; ++i)
        set.add(i);
    return set;
}

function values(set) {
    var result = [];
    set.forEach(function (value, key) { result.push(key + ':' + value); });
    return result.join(',');
}
noInline(values);

function deleteAhead(set) {
    var result = [];
    set.forEach(function (value) {
        result.push(value);
        if (value % 2 == 0)
            set.delete(value + 1);
    });
    return result.join(',');
}
noInline(deleteAhead);

function deleteAllAhead(set) {
    var result = [];
    set.forEach(function (value) {
        result.push(value);
        if (value == 2) {
            for (var i = 3; i < 10; ++i)
                set.delete(i);
        }
    });
    return result.join(',');
}
noInline(deleteAllAhead);

function addDuring(set) {
    var result = [];
    var added = 0;
    set.forEach(function (value) {
        result.push(value);
        if (added < 40)
            set.add('x' + added++);
    });
    return result.length;
}
noInline(addDuring);

function clearDuring(set) {
    var result = [];
    set.forEach(function (value) {
        result.push(value);
        if (value == 3)
            set.clear();
    });
    return result.join(',');
}
noInline(clearDuring);

function clearAndReadd(set) {
    var result = [];
    set.forEach(function (value) {
        result.push(value);
        if (value == 3) {
            set.clear();
            set.add('a');
            set.add('b');
        }
    });
    return result.join(',');
}
noInline(clearAndReadd);

for (var i = 0; i < testLoopCount; ++i) {
    shouldBe(values(new Set), '');
    var set = makeSet(5);
    set.delete(0);
    set.delete(4);
    shouldBe(values(set), '1:1,2:2,3:3');
    set = new Set([1]);
    set.delete(1);
    shouldBe(values(set), '');
    shouldBe(deleteAhead(makeSet(10)), '0,2,4,6,8');
    shouldBe(deleteAllAhead(makeSet(10)), '0,1,2');
    shouldBe(addDuring(makeSet(3)), 43);
    shouldBe(clearDuring(makeSet(10)), '0,1,2,3');
    shouldBe(clearAndReadd(makeSet(10)), '0,1,2,3,a,b');
}
