//@ requireOptions("--useArrayGroupByMethod=1")

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
    Array.prototype.groupBy.call(null, () => { /* empty */ })
}, `TypeError: Array.prototype.groupBy requires that |this| not be null or undefined`);
shouldThrow(() => {
    Array.prototype.groupBy.call(undefined, () => { /* empty */ })
}, `TypeError: Array.prototype.groupBy requires that |this| not be null or undefined`);

shouldThrow(() => {
    Array.prototype.groupByToMap.call(null, () => { /* empty */ })
}, `TypeError: Array.prototype.groupByToMap requires that |this| not be null or undefined`);
shouldThrow(() => {
    Array.prototype.groupByToMap.call(undefined, () => { /* empty */ })
}, `TypeError: Array.prototype.groupByToMap requires that |this| not be null or undefined`);
