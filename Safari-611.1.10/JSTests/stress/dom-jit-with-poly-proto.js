var createDOMJITGetterBaseJSObject =  $vm.createDOMJITGetterBaseJSObject;

function assert(b, m) {
    if (!b)
        throw new Error("Bad:" + m);
}

function makePolyProtoObject() {
    function foo() {
        class C {
            constructor() {
                this._field = 25;
            }
        };
        return new C;
    }
    for (let i = 0; i < 15; ++i)
        foo();
    return foo();
}

let proto = createDOMJITGetterBaseJSObject();
let obj = makePolyProtoObject();
obj.__proto__ = proto;

function validate(x, v) {
    assert(x.customGetter === v, x.customGetter);
}
noInline(validate);

for (let i = 0; i < 1000; ++i)
    validate(obj, proto);

proto.foo = 25;
for (let i = 0; i < 1000; ++i)
    validate(obj, proto);

Reflect.setPrototypeOf(obj, {});
for (let i = 0; i < 1000; ++i) {
    validate(obj, undefined);
}
