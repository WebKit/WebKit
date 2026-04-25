function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var object = {};
var properties = [];

for (var i = 0; i < 128; ++i) {
    var key = "Hey" + i;
    properties.push(key);
    object[key] = i;
}
object["MakeNonCompact"] = true;
properties.push("MakeNonCompact");
shouldBe(JSON.stringify(Object.keys(object)), JSON.stringify(properties));
for (var i = 0; i < 128; ++i) {
    var key = "Hey" + i;
    shouldBe(object[key], i);
}
shouldBe(object["MakeNonCompact"], true);
