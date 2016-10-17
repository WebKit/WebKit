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

var complex = createDOMJITGetterComplexObject();
var object = {};
object.__proto__ = complex;
function access(object)
{
    return object.customGetter;
}
noInline(access);
for (var i = 0; i < 1e4; ++i) {
    shouldThrow(() => {
        access(object);
    }, `TypeError: Type error`);
}
