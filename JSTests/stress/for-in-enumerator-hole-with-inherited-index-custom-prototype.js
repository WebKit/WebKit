function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: " + actual + " expected: " + expected);
}

function mutate(array, proto, key, doMutate) {
    if (doMutate && key === "1") {
        delete array[3];
        proto[3] = "inherited";
    }
}
noInline(mutate);

function test(array, proto, doMutate) {
    var keys = [];
    for (var key in array) {
        keys.push(key);
        mutate(array, proto, key, doMutate);
    }
    return keys.join(",");
}
noInline(test);

function makeArray() {
    var proto = {};
    var array = [1, 2, 3, 4, 5];
    Object.setPrototypeOf(array, proto);
    return [array, proto];
}

for (var i = 0; i < testLoopCount; i++) {
    var [array, proto] = makeArray();
    shouldBe(test(array, proto, false), "0,1,2,3,4");
}

var [array, proto] = makeArray();
shouldBe(test(array, proto, true), "0,1,2,3,4");
