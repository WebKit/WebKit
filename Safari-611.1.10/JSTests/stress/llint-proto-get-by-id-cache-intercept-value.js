let p = Object.create({ foo: 1 });
let o = Object.create(p);

let other = { foo: 10 };

function foo() {
    return o.foo
}

for (let i = 0; i < 10; i++)
    foo();

p.foo = null;
gc();

if (foo() !== null)
    throw new Error("bad get by id access");
