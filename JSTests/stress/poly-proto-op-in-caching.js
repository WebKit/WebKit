function assert(b, m) {
    if (!b)
        throw new Error("Bad:" + m);
}

function makePolyProtoObject() {
    function foo() {
        class C {
            constructor() {
                this.field = 42;
            }
        };
        return new C;
    }
    for (let i = 0; i < 15; ++i) {
        assert(foo().field === 42);
    }
    return foo();
}

function validate(o, b) {
    assert("x" in o === b);
}
noInline(validate);

let start = Date.now();

let objs = [];
for (let i = 0; i < 10; ++i)
    objs.push(makePolyProtoObject());

objs.forEach(obj => Reflect.setPrototypeOf(obj, {x:20}));

for (let i = 0; i < 10000; ++i) {
    for (let obj of objs)
        validate(obj, true);
}

objs.forEach(obj => Reflect.setPrototypeOf(obj, {}));
for (let i = 0; i < 10000; ++i) {
    for (let obj of objs)
        validate(obj, false);
}


function validate2(o, b) {
    assert("x" in o === b);
}
noInline(validate2);

objs.forEach(obj => Reflect.setPrototypeOf(obj, null));
for (let i = 0; i < 10000; ++i) {
    for (let obj of objs)
        validate2(obj, false);
}

objs.forEach(obj => Reflect.setPrototypeOf(obj, {x:25}));
for (let i = 0; i < 10000; ++i) {
    for (let obj of objs)
        validate2(obj, true);
}

if (false)
    print(Date.now() - start);
