description("Tests to make sure we call forEach callback with right arguments");
var m = new Map();
m.set('key', 'value');
var called = false;
var actual = {};
var receiver = { receiver: true };
m.forEach(function (value, key, map) {
    called = true;
    actual.value = value;
    actual.key = key;
    actual.map = map;
    actual.receiver = this;
}, receiver);
shouldBeTrue("called");
shouldBe("actual.value", "'value'");
shouldBe("actual.key", "'key'");
shouldBe("actual.map", "m");
shouldBe("actual.receiver", "receiver");
