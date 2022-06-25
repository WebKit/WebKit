var createCustomTestGetterSetter = $vm.createCustomTestGetterSetter;

function assert(b, m) {
    if (!b)
        throw new Error("Bad:" + m);
}

function makePolyProtoObject() {
    function foo() {
        class C { 
            constructor() { this.field = 20; }
        };
        return new C;
    }
    for (let i = 0; i < 15; ++i) {
        assert(foo().field === 20);
    }
    return foo();
}

let items = [
    makePolyProtoObject(),
    makePolyProtoObject(),
    makePolyProtoObject(),
    makePolyProtoObject(),
    makePolyProtoObject(),
    makePolyProtoObject(),
    makePolyProtoObject(),
    makePolyProtoObject(),
    makePolyProtoObject(),
    makePolyProtoObject(),
    makePolyProtoObject(),
    makePolyProtoObject(),
    makePolyProtoObject(),
    makePolyProtoObject(),
    makePolyProtoObject(),
    makePolyProtoObject(),
];

let customGetterSetter = createCustomTestGetterSetter();
items.forEach((x) => {
    x.__proto__ = customGetterSetter;
    assert(x.__proto__ === customGetterSetter);
});

function validate(x, valueResult, accessorResult) {
    assert(x.customValue === valueResult);

    assert(x.customAccessor === accessorResult);

    let o = {};
    x.customValue = o;
    assert(o.result === valueResult);

    o = {};
    x.customAccessor = o;
    assert(o.result === accessorResult);

    assert(x.randomProp === 42 || x.randomProp === undefined);
}
noInline(validate);


let start = Date.now();
for (let i = 0; i < 10000; ++i) {
    for (let i = 0; i < items.length; ++i) {
        validate(items[i], customGetterSetter, items[i]);
    }
}

customGetterSetter.randomProp = 42;

for (let i = 0; i < 10000; ++i) {
    for (let i = 0; i < items.length; ++i) {
        validate(items[i], customGetterSetter, items[i]);
    }
}

items.forEach((x) => {
    Reflect.setPrototypeOf(x, {
        get customValue() { return 42; },
        get customAccessor() { return 22; },
        set customValue(x) { x.result = 42; },
        set customAccessor(x) { x.result = 22; },
    });
});

for (let i = 0; i < 10000; ++i) {
    for (let i = 0; i < items.length; ++i) {
        validate(items[i], 42, 22);
    }
}

if (false)
    print(Date.now() - start);
