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

shouldThrow(() => {
    let registry = new FinalizationRegistry(function() { });
    registry.register(Symbol.for("Hey"));
}, `TypeError: register requires an object or a non-registered symbol as the target`);

shouldThrow(() => {
    let registry = new FinalizationRegistry(function() { });
    registry.register(Symbol(), null, Symbol.for("Hey"));
}, `TypeError: register requires an object or a non-registered symbol as the unregistration token`);

shouldThrow(() => {
    let registry = new FinalizationRegistry(function() { });
    registry.unregister(Symbol.for("Hey"));
}, `TypeError: unregister requires an object or a non-registered symbol as the unregistration token`);
