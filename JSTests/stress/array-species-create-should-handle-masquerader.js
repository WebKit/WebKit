function shouldThrow(func, errorMessage) {
    var errorThrown = false;
    var error = null;
    try {
        func();
    } catch (e) {
        errorThrown = true;
        error = e;
    }
    if (!errorThrown)
        throw new Error('not thrown');
    if (String(error) !== errorMessage)
        throw new Error(`bad error: ${String(error)}`);
}
noInline(shouldThrow);

for (var i = 0; i < 1e5; ++i) {
    shouldThrow(() => {
        new (class extends Array { static get [Symbol.species]() { return makeMasquerader(); } })(1, 2, 3).flat().constructor
    }, `TypeError: Masquerader is not a constructor`);
}
