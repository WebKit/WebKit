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

var r = Proxy.revocable({}, {});
r.revoke();

shouldThrow(() => {
    Array.isArray(r.proxy);
}, `TypeError: Array.isArray cannot be called on a Proxy that has been revoked`);

shouldThrow(() => {
    Object.prototype.toString.call(r.proxy);
}, `TypeError: Object.prototype.toString cannot be called on a Proxy that has been revoked`);
