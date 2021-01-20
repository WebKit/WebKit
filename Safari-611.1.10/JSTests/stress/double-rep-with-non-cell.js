// Only bool, undefined and null
function addNullBoolUndefined(a, b) {
    return a + b;
}
noInline(addNullBoolUndefined);

for (var i = 0; i < 1e4; ++i) {
    var value = addNullBoolUndefined(0.5, null);
    if (value !== 0.5)
        throw "addNullBoolUndefined(0.5, null) failed with i = " + i + " returned value = " + value;

    var value = addNullBoolUndefined(null, undefined);
    if (value === value)
        throw "addNullBoolUndefined(null, undefined) failed with i = " + i + " returned value = " + value;

    var value = addNullBoolUndefined(true, null);
    if (value !== 1)
        throw "addNullBoolUndefined(true, null) failed with i = " + i + " returned value = " + value;

    var value = addNullBoolUndefined(undefined, false);
    if (value === value)
        throw "addNullBoolUndefined(undefined, false) failed with i = " + i + " returned value = " + value;

    var value = addNullBoolUndefined(false, true);
    if (value !== 1)
        throw "addNullBoolUndefined(false, true) failed with i = " + i + " returned value = " + value;

    var value = addNullBoolUndefined(null, 42);
    if (value !== 42)
        throw "addNullBoolUndefined(null, 42) failed with i = " + i + " returned value = " + value;

}
