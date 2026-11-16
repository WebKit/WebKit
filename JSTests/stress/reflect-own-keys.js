function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function shouldThrow(func, message) {
    var error = null;
    try {
        func();
    } catch (e) {
        error = e;
    }
    if (!error)
        throw new Error("not thrown.");
    if (String(error) !== message)
        throw new Error("bad error: " + String(error));
}

function shouldBeArray(actual, expected) {
    shouldBe(actual.length, expected.length);
    for (var i = 0; i < expected.length; ++i) {
        try {
            shouldBe(actual[i], expected[i]);
        } catch(e) {
            print(JSON.stringify(actual));
            throw e;
        }
    }
}

shouldBe(Reflect.ownKeys.length, 1);

shouldThrow(() => {
    Reflect.ownKeys("hello");
}, `TypeError: Reflect.ownKeys requires the first argument be an object`);

var cocoa = Symbol("Cocoa");
var cappuccino = Symbol("Cappuccino");

shouldBeArray(Reflect.ownKeys({}), []);
shouldBeArray(Reflect.ownKeys({42:42}), ['42']);
shouldBeArray(Reflect.ownKeys({0:0,1:1,2:2}), ['0','1','2']);
shouldBeArray(Reflect.ownKeys({0:0,1:1,2:2,hello:42}), ['0','1','2','hello']);
shouldBeArray(Reflect.ownKeys({hello:42,0:0,1:1,2:2,world:42}), ['0','1','2','hello','world']);
shouldBeArray(Reflect.ownKeys({[cocoa]:42,hello:42,0:0,1:1,2:2,world:42}), ['0','1','2','hello','world', cocoa]);
shouldBeArray(Reflect.ownKeys({[cocoa]:42,hello:42,0:0,1:1,2:2,[cappuccino]:42,world:42}), ['0','1','2','hello','world', cocoa, cappuccino]);
