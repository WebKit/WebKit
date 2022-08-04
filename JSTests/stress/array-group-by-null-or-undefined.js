//@ requireOptions("--useArrayGroupMethod=1")

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
    Array.prototype.group.call(null, () => { /* empty */ })
}, `TypeError: Array.prototype.group requires that |this| not be null or undefined`);
shouldThrow(() => {
    Array.prototype.group.call(undefined, () => { /* empty */ })
}, `TypeError: Array.prototype.group requires that |this| not be null or undefined`);

shouldThrow(() => {
    Array.prototype.groupToMap.call(null, () => { /* empty */ })
}, `TypeError: Array.prototype.groupToMap requires that |this| not be null or undefined`);
shouldThrow(() => {
    Array.prototype.groupToMap.call(undefined, () => { /* empty */ })
}, `TypeError: Array.prototype.groupToMap requires that |this| not be null or undefined`);
