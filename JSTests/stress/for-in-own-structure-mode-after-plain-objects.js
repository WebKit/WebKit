function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: " + actual + ", expected: " + expected);
}

var symbol = Symbol("symbol");

function get(object) {
    var result = "";
    for (var key in object)
        result += key + ":" + object[key] + ",";
    return result;
}
noInline(get);

function put(object) {
    for (var key in object)
        object[key] = 7;
}
noInline(put);

function getFromOther(object, other) {
    var result = "";
    for (var key in object)
        result += key + ":" + other[key] + ",";
    return result;
}
noInline(getFromOther);

function putToOther(object, other) {
    for (var key in object)
        other[key] = 7;
}
noInline(putToOther);

for (var i = 0; i < testLoopCount; ++i) {
    var plain = { a: i, b: 2, c: 3 };
    shouldBe(get(plain), "a:" + i + ",b:2,c:3,");
    shouldBe(getFromOther(plain, plain), "a:" + i + ",b:2,c:3,");
    put(plain);
    shouldBe(get(plain), "a:7,b:7,c:7,");
    putToOther(plain, plain);
}

for (var i = 0; i < testLoopCount; ++i) {
    var withSymbol = { a: i, [symbol]: 50, b: 2, c: 3 };
    shouldBe(get(withSymbol), "a:" + i + ",b:2,c:3,");
    put(withSymbol);
    shouldBe(get(withSymbol), "a:7,b:7,c:7,");
    shouldBe(withSymbol[symbol], 50);

    var stored = 0;
    var withAccessor = { a: i, get b() { return 2; }, set b(value) { stored = value; }, c: 3 };
    shouldBe(get(withAccessor), "a:" + i + ",b:2,c:3,");
    put(withAccessor);
    shouldBe(get(withAccessor), "a:7,b:2,c:7,");
    shouldBe(stored, 7);

    var plain = { a: i, b: 2, c: 3 };
    var reordered = { c: 3, b: 2, a: i };
    shouldBe(getFromOther(plain, reordered), "a:" + i + ",b:2,c:3,");
    putToOther(plain, reordered);
    shouldBe(get(reordered), "c:7,b:7,a:7,");
    shouldBe(get(plain), "a:" + i + ",b:2,c:3,");

    shouldBe(getFromOther(withSymbol, plain), "a:" + i + ",b:2,c:3,");
    shouldBe(getFromOther(withSymbol, 1), "a:undefined,b:undefined,c:undefined,");
}
