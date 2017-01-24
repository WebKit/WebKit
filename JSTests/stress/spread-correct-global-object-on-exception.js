function assert(b) {
    if (!b)
        throw new Error("Bad assertion");
}
function spread(a) {
    return [...a];
}
noInline(spread);

const objectText = `
    let o = {
        [Symbol.iterator]() {
            return {
                next() {
                    return {done: true};
                }
            };
        }
    };
    o;
`;

let o = eval(objectText);
for (let i = 0; i < 1000; i++) {
    if (i % 23 === 0)
        o = eval(objectText);
    spread(o);
}

let myGlobalObject = globalObjectForObject(new Object);

let secondGlobalObject = createGlobalObject();
let o2 = secondGlobalObject.Function("return {};")();

let error = null;
try {
    spread(o2);
} catch(e) {
    error = e;
}

assert(error);
assert(globalObjectForObject(error) === myGlobalObject);
