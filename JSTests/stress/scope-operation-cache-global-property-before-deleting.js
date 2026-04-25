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

function bar()
{
    foo = 42;
}

bar();
bar();
delete globalThis.foo;
$.evalScript(`const foo = 50`);

shouldThrow(() => bar(), `TypeError: Attempted to assign to readonly property.`);
