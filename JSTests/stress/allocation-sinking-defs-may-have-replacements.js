function dontCSE() { }
noInline(dontCSE);

function assert(b) {
    if (!b)
        throw new Error("Bad assertion");
}
noInline(assert);

function foo(a1) {
    let o1 = {x: 20, y: 50};
    let o2 = {y: 40, o1: o1};
    let o3 = {};

    o3.field = o1.y;

    dontCSE();

    if (a1) {
        a1 = true; 
    } else {
        a1 = false;
    }

    let value = o3.field;
    assert(value === 50);
}
noInline(foo);

for (let i = 0; i < 100000; i++)
    foo(i);
