function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function makePolyProtoObject() {
    function foo() {
        class C {
            constructor() {
                this._field = 42;
            }
        };
        return new C;
    }
    for (let i = 0; i < 15; ++i)
        foo();
    return foo();
}

function target(object)
{
    return object.__proto__;
}
noInline(target);

var polyProtoObject = makePolyProtoObject();
var prototype = Reflect.getPrototypeOf(polyProtoObject);
for (var i = 0; i < 1e5; ++i)
    shouldBe(target(polyProtoObject), prototype);
