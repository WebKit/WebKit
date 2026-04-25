var setGlobalConstRedeclarationShouldNotThrow = $vm.setGlobalConstRedeclarationShouldNotThrow;

function assert(b) {
    if (!b)
        throw new Error("Bad assertion.");
}

setGlobalConstRedeclarationShouldNotThrow(); // Allow duplicate const declarations at the global level.

load("./global-const-redeclaration-setting/let.js", "caller relative");
assert(foo === 50);
let threw = false;
try {
    load("./global-const-redeclaration-setting/first.js", "caller relative"); // Redeclaration of a 'let' to 'const' should always throw because it isn't breaking backwards compat.
} catch(e) {
    threw = true;
}

assert(threw);
assert(foo === 50);
