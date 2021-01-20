function assert(b) {
    if (!b)
        throw new Error("Bad assertion");
}
function spread(a) {
    return [...a];
}
noInline(spread);

const foo = {};

let secondGlobalObject = createGlobalObject();
secondGlobalObject.Array.prototype[0] = foo;
let x = secondGlobalObject.Function("return [, 20];")();
let result = spread(x);
assert(result.length === 2);
assert(result[0] === foo);
assert(result[1] === 20);
