function assert(x) {
    if (!x)
        throw new Error("Bad assertion!");
}

class Foo {
    static * staticGen(arg = (Foo.staticGen.prototype = 1)) {}
    async * asyncGen(arg = (Foo.prototype.asyncGen.prototype = true)) {}
}

const staticGenOriginalPrototype = Foo.staticGen.prototype;
const asyncGenOriginalPrototype = Foo.prototype.asyncGen.prototype;

assert(Foo.staticGen().__proto__ !== staticGenOriginalPrototype);
assert(Foo.prototype.asyncGen().__proto__ !== asyncGenOriginalPrototype);
