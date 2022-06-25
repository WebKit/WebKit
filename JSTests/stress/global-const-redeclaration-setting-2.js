var setGlobalConstRedeclarationShouldNotThrow = $vm.setGlobalConstRedeclarationShouldNotThrow;

function assert(b) {
    if (!b)
        throw new Error("Bad assertion.");
}

setGlobalConstRedeclarationShouldNotThrow(); // Allow duplicate const declarations at the global level.

for (let i = 0; i < 100; i++) {
    load("./global-const-redeclaration-setting/first.js", "caller relative");
    assert(foo === 20);
    load("./global-const-redeclaration-setting/second.js", "caller relative");
    assert(foo === 40);
}
