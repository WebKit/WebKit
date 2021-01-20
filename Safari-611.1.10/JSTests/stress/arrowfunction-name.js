function assert(b) {
    if (!b)
        throw new Error("Bad assertion")
}

// Anonymous
assert((()=>{}).name === "");

// Inferred name with global variable.
f = () => {};
assert(f.name === "f");

// Inferred name with variable declaration.
let lf = () => {};
assert(lf.name === "lf");
