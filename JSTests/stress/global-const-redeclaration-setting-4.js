function assert(b) {
    if (!b)
        throw new Error("Bad assertion.");
}

setGlobalConstRedeclarationShouldNotThrow(); // Allow duplicate const declarations at the global level.

load("./global-const-redeclaration-setting/first.js");
assert(foo === 20);
let threw = false;
try {
    load("./global-const-redeclaration-setting/let.js"); // Redeclaration a 'let' variable should throw because this doesn't break backwards compat.
} catch(e) {
    threw = true;
}

assert(threw);
assert(foo === 20);
