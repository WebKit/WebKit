function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: " + actual + ", expected: " + expected);
}

function test(object) {
    var result = "";
    for (var key in object)
        result += key + ":" + object[key] + ",";
    return result;
}
noInline(test);

var symbol = Symbol("symbol");

for (var i = 0; i < testLoopCount; ++i) {
    var withSymbol = $vm.createImpureGetter({ a: 10, b: 11 });
    withSymbol.a = i;
    withSymbol[symbol] = 50;
    withSymbol.b = 2;
    shouldBe(test(withSymbol), "a:10,b:11,");

    var deleted = $vm.createImpureGetter({ a: 10, b: 11 });
    deleted.a = i;
    deleted.parent = 0;
    deleted.b = 2;
    delete deleted.parent;
    shouldBe(test(deleted), "a:10,b:11,");
}

var hiddenDelegate = { b: 11 };
Object.defineProperty(hiddenDelegate, "a", { value: 10, enumerable: false });

for (var i = 0; i < testLoopCount; ++i) {
    var hiddenByDelegate = $vm.createImpureGetter(hiddenDelegate);
    hiddenByDelegate.a = i;
    hiddenByDelegate[symbol] = 50;
    hiddenByDelegate.b = 2;
    shouldBe(test(hiddenByDelegate), "b:11,");
}
