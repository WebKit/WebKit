//@ requireOptions("--forcePolyProto=1")
function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test(object) {
    var result = []
    for (var k in object)
        result.push(k);
    return JSON.stringify(result);
}
noInline(test);

var constructors = []
Object.prototype.hey = 32;
function factory() {
    function Test() { }
    constructors.push(Test);
    return new Test;
}

var object = factory();
shouldBe(test(object), `["hey"]`);
shouldBe(test(object), `["hey"]`);
shouldBe(test(object), `["hey"]`);
var object2 = factory();
shouldBe(test(object2), `["hey"]`);
shouldBe(test(object2), `["hey"]`);
shouldBe(test(object2), `["hey"]`);
object2.__proto__ = { ok: 33 };
shouldBe(test(object2), `["ok","hey"]`);
shouldBe(test(object2), `["ok","hey"]`);
shouldBe(test(object2), `["ok","hey"]`);
