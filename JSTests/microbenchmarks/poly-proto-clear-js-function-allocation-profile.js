function assert(b) {
    if (!b)
        throw new Error("Bad assertion")
}

function foo() {
    class Foo {
        ensureX() {
            if (!this.x)
                this.x = 22;
            return this;
        }
    };
    return Foo;
}

function access(o) {
    return assert(o.ensureX().x === 22);
}
noInline(access);

let ctors = [];

for (let i = 0; i < 50; ++i) {
    let ctor = foo();
    new ctor; // warm things up
    ctors.push(ctor);
}

let start = Date.now();
for (let i = 0; i < 5000; ++i) {
    for (let i = 0; i < ctors.length; ++i)
        access(new ctors[i]);
}
if (false)
    print(Date.now() - start);
