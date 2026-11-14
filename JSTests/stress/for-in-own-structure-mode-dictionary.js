function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: " + actual + ", expected: " + expected);
}

var symbol = Symbol("symbol");

function create(i) {
    return { a: i, [symbol]: 50, b: 2, c: 3 };
}

function deleteLater(object) {
    var result = "";
    for (var key in object) {
        if (key === "a")
            delete object.b;
        result += key + ":" + object[key] + ":" + object.hasOwnProperty(key) + ",";
    }
    return result;
}
noInline(deleteLater);

function hideLater(object) {
    var result = "";
    for (var key in object) {
        if (key === "a")
            Object.defineProperty(object, "b", { enumerable: false });
        result += key + ":" + object[key] + ":" + (key in object) + ",";
    }
    return result;
}
noInline(hideLater);

function deleteCurrent(object) {
    var result = "";
    for (var key in object) {
        delete object[key];
        result += key + ":" + object[key] + ":" + object.hasOwnProperty(key) + ":" + (key in object) + ",";
    }
    return result;
}
noInline(deleteCurrent);

for (var i = 0; i < testLoopCount / 2; ++i) {
    for (var convert of [$vm.toCacheableDictionary, $vm.toUncacheableDictionary]) {
        var object = create(i);
        convert(object);
        shouldBe(deleteLater(object), "a:" + i + ":true,c:3:true,");

        object = create(i);
        convert(object);
        shouldBe(hideLater(object), "a:" + i + ":true,c:3:true,");

        object = create(i);
        convert(object);
        shouldBe(deleteCurrent(object), "a:undefined:false:false,b:undefined:false:false,c:undefined:false:false,");
    }
}
