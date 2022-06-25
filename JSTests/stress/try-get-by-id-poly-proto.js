var createBuiltin = $vm.createBuiltin;
var loadGetterFromGetterSetter = $vm.loadGetterFromGetterSetter;

function assert(b, m) {
    if (!b)
        throw new Error("Bad:" + m);
}

function makePolyProtoObject() {
    function foo() {
        class C {
            constructor() { this._field = 42; }
        };
        return new C;
    }
    for (let i = 0; i < 15; ++i)
        foo();
    return foo();
}

function tryGetByIdText(propertyName) { return `(function (base) { return @tryGetById(base, '${propertyName}'); })`; }
let getFoo = createBuiltin(tryGetByIdText("foo"));
let getBar = createBuiltin(tryGetByIdText("bar"));
let getNonExistentField = createBuiltin(tryGetByIdText("nonExistentField"));

let x = makePolyProtoObject();
x.__proto__ = { foo: 42, get bar() { return 22; } };
let barGetter = Object.getOwnPropertyDescriptor(x.__proto__, "bar").get;
assert(typeof barGetter === "function");
assert(barGetter() === 22);

function validate(x) {
    assert(getFoo(x) === 42);
    assert(loadGetterFromGetterSetter(getBar(x)) === barGetter);
    assert(getNonExistentField(x) === undefined);
}
noInline(validate);

for (let i = 0; i < 1000; ++i) {
    validate(x);
}
