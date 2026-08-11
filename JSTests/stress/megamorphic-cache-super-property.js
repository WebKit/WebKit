function makeProtoObject(val) {
    let obj = { x: val };
    Object.create(obj);
    return obj;
}

let obj1 = makeProtoObject(42);
let obj2 = makeProtoObject(100);

function readProp(obj) { return obj.x; }
noInline(readProp);

// Drive readProp IC to megamorphic.
for (let i = 0; i < 200; i++) { let o = { x: i }; o['w' + i] = i; Object.create(o); readProp(o); }
for (let i = 0; i < 5000; i++) readProp(makeProtoObject(i));

// Drive super accessor IC to megamorphic.
class Base { readSuper() { return super.x; } }
for (let i = 0; i < 200; i++) {
    let p = { x: i }; p['s' + i] = i; Object.create(p);
    Object.setPrototypeOf(Base.prototype, p);
    new Base().readSuper();
}

// Access super.x through obj1, then read obj2.x via regular GetById.
gc();
Object.setPrototypeOf(Base.prototype, obj1);
new Base().readSuper();

let result = readProp(obj2);
if (result !== 100)
    throw new Error("Expected 100 but got " + result);
