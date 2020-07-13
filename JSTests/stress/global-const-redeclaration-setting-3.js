var setGlobalConstRedeclarationShouldNotThrow = $vm.setGlobalConstRedeclarationShouldNotThrow;

function assert(b) {
    if (!b)
        throw new Error("Bad assertion.");
}

setGlobalConstRedeclarationShouldNotThrow(); // Allow duplicate const declarations at the global level.

load("./global-const-redeclaration-setting/first.js", "caller relative");
assert(foo === 20);
let threw = false;
try {
    load("./global-const-redeclaration-setting/strict.js", "caller relative"); // We ignore the setting and always throw an error when in strict mode!
} catch(e) {
    threw = true;
}

assert(threw);
assert(foo === 20);
