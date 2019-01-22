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

function foo() {
    bar = 4;
}
Object.preventExtensions(this);
foo();
$.evalScript('const bar = 3;');
shouldThrow(() => foo(), `TypeError: Attempted to assign to readonly property.`);
