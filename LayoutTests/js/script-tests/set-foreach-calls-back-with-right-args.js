description("Tests to make sure we call forEach callback with right arguments");
var s = new Set();
s.add('value');
var called = false;
var receiver = { receiver: true };
var actual = {};
s.forEach(function (value, key, set) {
    called = true;
    actual.value = value;
    actual.key = key;
    actual.set = set;
    actual.receiver = this;
}, receiver);
shouldBeTrue("called");
shouldBe("actual.value", "'value'");
shouldBe("actual.key", "'value'");
shouldBe("actual.set", "s");
shouldBeTrue("actual.receiver === receiver");
