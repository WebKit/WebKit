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
    let realm = new ShadowRealm();
    let f = realm.evaluate(`new Proxy(()=>{}, {});`);
    f({});
}, `TypeError: value passing between realms must be callable or primitive`);
