function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: " + actual + " expected: " + expected);
}

function find(table, key) {
    var entry = table[key & (table.length - 1)];
    while (entry && entry.key !== key)
        entry = entry.next;
    return entry;
}

function get(table, key) {
    var entry = find(table, key);
    if (entry)
        return entry.value;
    return -1;
}

function test(table, n) {
    var sum = 0;
    for (var i = 0; i < n; ++i)
        sum += get(table, i);
    return sum;
}
noInline(test);

function truthiness(value) {
    if (value)
        return typeof value;
    return "falsy:" + String(value);
}
noInline(truthiness);

function objectOrNull(value) {
    if (value)
        return value.x;
    return -1;
}
noInline(objectOrNull);

function nullOrUndefined(c) {
    var value = c ? null : undefined;
    if (value)
        return value.x;
    return -1;
}
noInline(nullOrUndefined);

var table = new Array(16);
for (var i = 0; i < 48; ++i)
    table[i & 15] = { key: i, value: i, next: table[i & 15] };

var expected = 0;
for (var i = 0; i < 48; ++i)
    expected += i;
expected -= 64 - 48;

var values = [null, undefined, 0, -0, "", false, NaN, 0n, 1, "a", true, {}, [], 1n];
var results = ["falsy:null", "falsy:undefined", "falsy:0", "falsy:0", "falsy:", "falsy:false", "falsy:NaN", "falsy:0", "number", "string", "boolean", "object", "object", "bigint"];
var object = { x: 42 };

for (var i = 0; i < testLoopCount; ++i) {
    shouldBe(test(table, 64), expected);
    for (var j = 0; j < values.length; ++j)
        shouldBe(truthiness(values[j]), results[j]);
    shouldBe(objectOrNull(i & 1 ? object : null), i & 1 ? 42 : -1);
    shouldBe(nullOrUndefined(i & 1), -1);
}

var masquerader = makeMasquerader();
masquerader.x = 7;
for (var i = 0; i < testLoopCount; ++i) {
    shouldBe(truthiness(masquerader).startsWith("falsy:"), true);
    shouldBe(objectOrNull(masquerader), -1);
    shouldBe(objectOrNull(object), 42);
}
