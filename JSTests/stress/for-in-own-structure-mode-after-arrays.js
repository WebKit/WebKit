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

for (var i = 0; i < testLoopCount / 2; ++i) {
    var array = [i, 2];
    array.a = 3;
    shouldBe(get(array), "0:" + i + ",1:2,a:3,");
    put(array);
    shouldBe(get(array), "0:7,1:7,a:7,");

    var plain = { a: i, b: 2, c: 3 };
    shouldBe(get(plain), "a:" + i + ",b:2,c:3,");
    put(plain);
    shouldBe(get(plain), "a:7,b:7,c:7,");
}

for (var i = 0; i < testLoopCount / 2; ++i) {
    var withSymbol = { a: i, [symbol]: 50, b: 2, c: 3 };
    shouldBe(get(withSymbol), "a:" + i + ",b:2,c:3,");
    put(withSymbol);
    shouldBe(get(withSymbol), "a:7,b:7,c:7,");
    shouldBe(withSymbol[symbol], 50);

    var deleted = { a: i, parent: 0, b: 2, c: 3 };
    delete deleted.parent;
    shouldBe(get(deleted), "a:" + i + ",b:2,c:3,");
    put(deleted);
    shouldBe(get(deleted), "a:7,b:7,c:7,");

    var stored = 0;
    var withAccessor = { a: i, get b() { return 2; }, set b(value) { stored = value; }, c: 3 };
    shouldBe(get(withAccessor), "a:" + i + ",b:2,c:3,");
    put(withAccessor);
    shouldBe(get(withAccessor), "a:7,b:2,c:7,");
    shouldBe(stored, 7);

    var arrayWithSymbol = [i, 2];
    arrayWithSymbol.a = 3;
    arrayWithSymbol[symbol] = 50;
    arrayWithSymbol.b = 4;
    shouldBe(get(arrayWithSymbol), "0:" + i + ",1:2,a:3,b:4,");
    put(arrayWithSymbol);
    shouldBe(get(arrayWithSymbol), "0:7,1:7,a:7,b:7,");
    shouldBe(arrayWithSymbol[symbol], 50);
}
