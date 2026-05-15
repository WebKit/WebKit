function shouldBe(a, b) {
    if (a !== b)
        throw new Error("expected " + b + " got " + a);
}

function makeShapes(count) {
    let shapes = [];
    for (let i = 0; i < count; i++) {
        let c = class {
            *[Symbol.iterator]() { yield i; }
        };
        c.prototype["p" + i] = i;
        shapes.push(new c());
    }
    return shapes;
}

const shapes = makeShapes(100);

// Symbol-only GetByVal at megamorphic site.
{
    function getSym(o) { return o[Symbol.iterator]; }
    noInline(getSym);
    for (let i = 0; i < 200; i++)
        for (let s of shapes)
            getSym(s);
    for (let i = 0; i < 10000; i++)
        shouldBe(typeof getSym([1, 2, 3]), "function");
}

// Symbol-only InByVal at megamorphic site.
{
    function inSym(o) { return Symbol.iterator in o; }
    noInline(inSym);
    for (let i = 0; i < 200; i++)
        for (let s of shapes)
            inSym(s);
    for (let i = 0; i < 10000; i++)
        shouldBe(inSym([1, 2, 3]), true);
    shouldBe(inSym(Object.create(null)), false);
}

// Symbol-only PutByVal at megamorphic site.
{
    let target = {};
    function putSym(o, v) { o[Symbol.toStringTag] = v; }
    noInline(putSym);
    for (let i = 0; i < 200; i++)
        for (let s of shapes)
            putSym(s, i);
    for (let i = 0; i < 10000; i++)
        putSym(target, "X" + i);
    shouldBe(target[Symbol.toStringTag], "X9999");
}

// Mixed String + Symbol at the same megamorphic site.
{
    const stringKeys = ["p0", "p1", "p2"];
    const symbolKeys = [Symbol.iterator, Symbol.toStringTag, Symbol.toPrimitive];
    function getMixed(o, k) { return o[k]; }
    function inMixed(o, k) { return k in o; }
    function putMixed(o, k, v) { o[k] = v; }
    noInline(getMixed);
    noInline(inMixed);
    noInline(putMixed);
    for (let i = 0; i < 200; i++) {
        for (let s of shapes) {
            for (let k of stringKeys) {
                getMixed(s, k);
                inMixed(s, k);
                putMixed(s, k, i);
            }
            for (let k of symbolKeys) {
                getMixed(s, k);
                inMixed(s, k);
                putMixed(s, k, i);
            }
        }
    }
    let arr = [1, 2, 3];
    for (let i = 0; i < 10000; i++) {
        shouldBe(getMixed(arr, "length"), 3);
        shouldBe(typeof getMixed(arr, Symbol.iterator), "function");
        shouldBe(inMixed(arr, "length"), true);
        shouldBe(inMixed(arr, Symbol.iterator), true);
    }
    let bag = {};
    for (let i = 0; i < 10000; i++) {
        putMixed(bag, "x", i);
        putMixed(bag, Symbol.toStringTag, i);
    }
    shouldBe(bag.x, 9999);
    shouldBe(bag[Symbol.toStringTag], 9999);
}

// Cache invalidation when Symbol-keyed prototype property changes.
{
    function getSym(o) { return o[Symbol.toPrimitive]; }
    noInline(getSym);
    let arr = [1, 2, 3];
    for (let i = 0; i < 200; i++)
        for (let s of shapes)
            getSym(s);
    for (let i = 0; i < 1000; i++)
        shouldBe(getSym(arr), undefined);
    Array.prototype[Symbol.toPrimitive] = function () { return "added"; };
    for (let i = 0; i < 1000; i++)
        shouldBe(typeof getSym(arr), "function");
    delete Array.prototype[Symbol.toPrimitive];
    for (let i = 0; i < 1000; i++)
        shouldBe(getSym(arr), undefined);
}

// Symbol value is non-cell at a mixed site (must fall to slow path, not crash).
{
    function getMixed(o, k) { return o[k]; }
    noInline(getMixed);
    for (let i = 0; i < 200; i++) {
        for (let s of shapes) {
            getMixed(s, "p0");
            getMixed(s, Symbol.iterator);
        }
    }
    let arr = [1, 2, 3];
    for (let i = 0; i < 10000; i++) {
        shouldBe(getMixed(arr, 0), 1);
        shouldBe(getMixed(arr, "length"), 3);
        shouldBe(typeof getMixed(arr, Symbol.iterator), "function");
    }
}
