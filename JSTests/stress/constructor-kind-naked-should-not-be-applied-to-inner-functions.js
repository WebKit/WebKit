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

new Promise((resolve) => {
    resolve(42);
});
drainMicrotasks();
shouldThrow(() => Promise(function () { }), `TypeError: Cannot call a constructor without |new|`)
