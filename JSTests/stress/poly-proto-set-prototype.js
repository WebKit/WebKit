function assert(b) {
    if (!b)
        throw new Error("bad");
}

let alternateProto = {
    get x() {
        return null;
    }
};

let alternateProto2 = {
    get y() { return 22; },
    get x() {
        return null;
    }
};

Object.defineProperty(Object.prototype, "x", {
    get: function() { return this._x; }
});

function foo() {
    class C {
        constructor() {
            this._x = 42;
        }
    };
    return new C;
}

function validate(o, p) {
    assert(o.x === p);
}
noInline(validate);

let arr = [];
foo();
for (let i = 0; i < 25; ++i)
    arr.push(foo());

for (let i = 0; i < 100; ++i) {
    for (let a of arr)
        validate(a, 42);
}

for (let a of arr) {
    a.__proto__ = alternateProto;
}
for (let i = 0; i < 100; ++i) {
    for (let a of arr) {
        validate(a, null);
    }
}

for (let a of arr) {
    a.__proto__ = alternateProto2;
}

for (let i = 0; i < 100; ++i) {
    for (let a of arr) {
        validate(a, null);
        assert(a.y === 22);
    }
}
