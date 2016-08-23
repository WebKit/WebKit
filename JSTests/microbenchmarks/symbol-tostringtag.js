// Create a really long prototype chain.

// We need to put values on so the objects are not empty and have transition
// watchpoints.
function buildChain(depth) {
    if (depth <= 0)
        return { bloop: 1 };
    let result = { value: 1 };
    Object.setPrototypeOf(result, buildChain(depth - 1));
    return result;
}


var object = buildChain(20);

function body() {
    for (let i = 0; i < 100000; i++)
        value = object.toString();
}
noInline(body);

// Try toString with misses.
body();

Object.prototype[Symbol.toStringTag] = "hit";

// Try toString with hit.
body();
