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

function create(i, change) {
    var object = { a: i, parent: 0, b: 2, c: 3 };
    delete object.parent;
    var changed = false;
    Object.setPrototypeOf(object, new Proxy({ inherited: 10 }, {
        getPrototypeOf(target) {
            if (!changed) {
                changed = true;
                change(object);
            }
            return Reflect.getPrototypeOf(target);
        }
    }));
    return object;
}

for (var i = 0; i < testLoopCount / 8; ++i) {
    var replaced = create(i, function (object) {
        delete object.b;
        object.x = 10;
        object.y = 11;
    });
    shouldBe(test(replaced), "a:" + i + ",c:3,inherited:10,");
    shouldBe(test(replaced), "a:" + i + ",c:3,x:10,y:11,inherited:10,");

    var deleted = create(i, function (object) {
        delete object.a;
        delete object.c;
    });
    shouldBe(test(deleted), "b:2,inherited:10,");

    var hidden = create(i, function (object) {
        Object.defineProperty(object, "b", { enumerable: false });
        object.parent = 4;
    });
    shouldBe(test(hidden), "a:" + i + ",c:3,inherited:10,");
}
