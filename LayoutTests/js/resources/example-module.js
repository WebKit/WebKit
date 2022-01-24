var value = true;

var check = function(log) {
    if (Object.getPrototypeOf(globalThis) !== Object.prototype) {
        log("globalThis prototype is Object.prototype");
        log(Object.getPrototypeOf(globalThis).toString());
        return false;
    }
    return true;
}

function log_equal(log, msg, x, y) {
    const result = x == y;
    log(`${msg}: ${result ? 'OK' : 'FAILED'}`);
}

var check_nested = function(log) {
    const sr = new ShadowRealm();
    globalThis.x = 100;
    sr.evaluate(`globalThis.x = 42`);
    const f = sr.evaluate(`() => {return globalThis.x;}`);
    log_equal(log, "calling nested realm wrapper uses correct global", 42, f());
    log_equal(log, "nested realm eval uses correct global", 42, sr.evaluate(`globalThis.x`));
    return true;
}

var setValue = function (newValue) {
    console.log(newValue);
    value = newValue;
}

export { value, setValue, check, check_nested };
