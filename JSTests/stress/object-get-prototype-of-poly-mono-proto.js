function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function makePolyProtoObject() {
    function foo() {
        class C {
            constructor() {
                this._field = 42;
                this.hello = 33;
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
    return [object.hello, Object.getPrototypeOf(object)];
}
noInline(target);

var polyProtoObject = makePolyProtoObject();
var prototype = Reflect.getPrototypeOf(polyProtoObject);
var object = { __proto__: prototype, hello: 44 };
for (var i = 0; i < 1e5; ++i) {
    shouldBe(target(polyProtoObject)[1], prototype);
    shouldBe(target(object)[1], prototype);
}
