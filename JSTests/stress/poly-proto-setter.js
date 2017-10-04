function assert(b, m) {
    if (!b)
        throw new Error("Bad:" + m);
}

let called = false;
function makePolyProtoObject() {
    function foo() {
        class C {
            constructor() 
            {
                this._p = null;
            }

            set p(x)
            {
                called = true;
                this._p = x;
            }
            get p()
            {
                return this._p;
            }
        };
        return new C;
    }
    for (let i = 0; i < 15; ++i) {
        assert(foo().p === null);
    }
    return foo();
}

function performSet(o) {
    o.p = 20;
}

let items = [];
for (let i = 0; i < 20; ++i) {
    items.push(makePolyProtoObject());
}

function performSet(x, i) {
    x.p = i;
}

let start = Date.now();
for (let i = 0; i < 100000; ++i) {
    for (let i = 0; i < items.length; ++i) {
        let o = items[i];
        performSet(o, i);
        assert(o._p === i);
        assert(called === true);
        called = false;
    }
}

items.forEach(o => {
    Reflect.setPrototypeOf(o, null);
});

for (let i = 0; i < 100000; ++i) {
    for (let i = 0; i < items.length; ++i) {
        let o = items[i];
        performSet(o, i);
        assert(o.p === i);
        assert(called === false);
    }
}

if (false)
    print(Date.now() - start);
