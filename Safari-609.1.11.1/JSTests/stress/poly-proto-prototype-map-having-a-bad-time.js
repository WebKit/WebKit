function assert(b) {
    if (!b)
        throw new Error("Bad assertion");
}

function foo() {
    class C {
        constructor() {
            this.x = 20;
        }
    };
    let item = new C;
    item[0] = 42;
    return [item, C.prototype];
}

for (let i = 0; i < 50; ++i)
    foo();

let [item, proto] = foo();
let called = false;
Object.defineProperty(proto, "1", {
    set(x) {
        called = true;
    }
});

assert(!called);
item[1] = 42;
assert(called);
