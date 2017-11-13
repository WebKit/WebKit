//@ runDefault

var exception;
var getter;

try {
    getter = loadGetterFromGetterSetter();
} catch (e) {
    exception = e;
}
if (exception != "TypeError: Invalid use of loadGetterFromGetterSetter test function: argument is not a GetterSetter")
    throw "FAILED";
if (getter)
    throw "FAILED: unexpected result";
exception = undefined;

try {
    getter = loadGetterFromGetterSetter(undefined);
} catch (e) {
    exception = e;
}
if (exception != "TypeError: Invalid use of loadGetterFromGetterSetter test function: argument is not a GetterSetter")
    throw "FAILED";
if (getter)
    throw "FAILED: unexpected result";
exception = undefined;

function tryGetByIdText(propertyName) { return `(function (base) { return @tryGetById(base, '${propertyName}'); })`; }
let getSetterGetter = createBuiltin(tryGetByIdText("bar"));

try {
    noGetterSetter = { };
    getter = loadGetterFromGetterSetter(getSetterGetter(noGetterSetter, "bar"));
} catch (e) {
    exception = e;
}
if (exception != "TypeError: Invalid use of loadGetterFromGetterSetter test function: argument is not a GetterSetter")
    throw "FAILED";
if (getter)
    throw "FAILED: unexpected result";
exception = undefined;

try {
    hasGetter = { get bar() { return 22; } };
    getter = loadGetterFromGetterSetter(getSetterGetter(hasGetter, "bar"));
} catch (e) {
    exception = e;
}
if (exception)
    throw "FAILED: unexpected exception: " + exception;
if (!getter)
    throw "FAILED: unable to get getter";

try {
    // When a getter is not specified, a default getter should be assigned as long as there's also a setter.
    hasSetter = { set bar(x) { return 22; } };
    getter = loadGetterFromGetterSetter(getSetterGetter(hasSetter, "bar"));
} catch (e) {
    exception = e;
}
if (exception)
    throw "FAILED: unexpected exception: " + exception;
if (!getter)
    throw "FAILED: unexpected result";
