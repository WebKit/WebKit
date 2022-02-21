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

function Test() { }
function factory() { return new Test; }

var object = factory();
shouldBe(test(object), `[]`);
shouldBe(test(object), `[]`);
shouldBe(test(object), `[]`);
object.ok = 42;
delete object.ok;
shouldBe(test(object), `[]`);
shouldBe(test(object), `[]`);
shouldBe(test(object), `[]`);
Test.prototype.ok = 42;
delete Test.prototype.ok;
shouldBe(test(object), `[]`);
shouldBe(test(object), `[]`);
shouldBe(test(object), `[]`);
Test.prototype.ok = 42;
test({});
test({});
shouldBe(test(object), `["ok"]`);
