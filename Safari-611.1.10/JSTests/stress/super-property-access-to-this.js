"use strict";
function assert(b) {
    if (!b)
        throw new Error("Bad assertion")
}

function test(f, n = 1000) {
    for (let i = 0; i < n; ++i)
        f();
}

class Base {
    get foo() { return this; }
}

class Child extends Base {
    a() {
        return super.foo;
    }

    b() {
        let arr = () => super.foo;
        return arr();
    }
};

let A = Child.prototype.a;
var AA = Child.prototype.a;
this.AAA = Child.prototype.a;

let globalObj = this;

test(function() {
    assert(Child.prototype.a.call("xyz") === "xyz");
    let obj = {};
    assert(Child.prototype.a.call(obj) === obj);
    assert(Child.prototype.a.call(25) === 25);
    assert(Child.prototype.a.call(globalObj) === globalObj);

    assert(Child.prototype.b.call("xyz") === "xyz");
    assert(Child.prototype.b.call(obj) === obj);
    assert(Child.prototype.b.call(25) === 25);
    assert(Child.prototype.b.call(globalObj) === globalObj);

    assert(A() === undefined);
    assert(AA() === undefined);
    assert(AAA() === undefined);
});
