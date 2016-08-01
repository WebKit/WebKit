// Using full number + undefined for math.
function addArgsNumberAndUndefined(a, b) {
    return a + b;
}
noInline(addArgsNumberAndUndefined);

for (var i = 0; i < 1e4; ++i) {
    var value = addArgsNumberAndUndefined(i, 1);
    if (value !== (i + 1))
        throw "addArgsNumberAndUndefined(i, 1) failed with i = " + i + " returned value = " + value;

    var value = addArgsNumberAndUndefined(0.5, i);
    if (value !== (i + 0.5))
        throw "addArgsNumberAndUndefined(0.5, i) failed with i = " + i + " returned value = " + value;

    var value = addArgsNumberAndUndefined(undefined, i);
    if (value === value)
        throw "addArgsNumberAndUndefined(undefined, i) failed with i = " + i + " returned value = " + value;

    var value = addArgsNumberAndUndefined(i, undefined);
    if (value === value)
        throw "addArgsNumberAndUndefined(i, undefined) failed with i = " + i + " returned value = " + value;

    var value = addArgsNumberAndUndefined(i);
    if (value === value)
        throw "addArgsNumberAndUndefined(i) failed with i = " + i + " returned value = " + value;

    var value = addArgsNumberAndUndefined(undefined, undefined);
    if (value === value)
        throw "addArgsNumberAndUndefined(undefined, undefined) failed with i = " + i + " returned value = " + value;

    var value = addArgsNumberAndUndefined();
    if (value === value)
        throw "addArgsNumberAndUndefined() failed with i = " + i + " returned value = " + value;
}


// Using int32 + undefined for math.
function addArgsInt32AndUndefined(a, b) {
    return a + b;
}
noInline(addArgsInt32AndUndefined);

for (var i = 0; i < 1e4; ++i) {
    var value = addArgsInt32AndUndefined(i, 1);
    if (value !== (i + 1))
        throw "addArgsInt32AndUndefined(i, 1) failed with i = " + i + " returned value = " + value;

    var value = addArgsInt32AndUndefined(undefined, i);
    if (value === value)
        throw "addArgsInt32AndUndefined(undefined, i) failed with i = " + i + " returned value = " + value;

    var value = addArgsInt32AndUndefined(i, undefined);
    if (value === value)
        throw "addArgsInt32AndUndefined(i, undefined) failed with i = " + i + " returned value = " + value;

    var value = addArgsInt32AndUndefined(i);
    if (value === value)
        throw "addArgsInt32AndUndefined(i) failed with i = " + i + " returned value = " + value;

    var value = addArgsInt32AndUndefined(undefined, undefined);
    if (value === value)
        throw "addArgsInt32AndUndefined(undefined, undefined) failed with i = " + i + " returned value = " + value;

    var value = addArgsInt32AndUndefined();
    if (value === value)
        throw "addArgsInt32AndUndefined() failed with i = " + i + " returned value = " + value;
}

function testFallbackWithDouble() {
    var value = addArgsNumberAndUndefined(Math.PI, Math.PI);
    if (value !== 2 * Math.PI)
        throw "addArgsNumberAndUndefined(Math.PI, Math.PI) failed with i = " + i + " returned value = " + value;
}
testFallbackWithDouble();


// Using full number + undefined for math.
function addArgsDoubleAndUndefined(a, b) {
    return a + b;
}
noInline(addArgsDoubleAndUndefined);

for (var i = 0; i < 1e4; ++i) {
    var value = addArgsDoubleAndUndefined(0.5, i);
    if (value !== (i + 0.5))
        throw "addArgsDoubleAndUndefined(0.5, i) failed with i = " + i + " returned value = " + value;

    var value = addArgsDoubleAndUndefined(undefined, 0.1);
    if (value === value)
        throw "addArgsDoubleAndUndefined(undefined, i) failed with i = " + i + " returned value = " + value;

    var value = addArgsDoubleAndUndefined(0.6, undefined);
    if (value === value)
        throw "addArgsDoubleAndUndefined(i, undefined) failed with i = " + i + " returned value = " + value;

    var value = addArgsDoubleAndUndefined(42.8);
    if (value === value)
        throw "addArgsDoubleAndUndefined(i) failed with i = " + i + " returned value = " + value;
}

function testFallbackWithObject() {
    var value = addArgsDoubleAndUndefined(Math.PI, { valueOf: function() { return 5; }});
    if (value !== 5 + Math.PI)
        throw "addArgsDoubleAndUndefined(Math.PI, { valueOf: function() { return 5; }}) failed with i = " + i + " returned value = " + value;
}
testFallbackWithObject();


// Using full number + undefined for math.
function addArgsOnlyUndefined(a, b) {
    return a + b;
}
noInline(addArgsOnlyUndefined);

for (var i = 0; i < 1e4; ++i) {
    var value = addArgsOnlyUndefined(undefined, undefined);
    if (value === value)
        throw "addArgsOnlyUndefined(undefined, undefined) failed with i = " + i + " returned value = " + value;

    var value = addArgsOnlyUndefined();
    if (value === value)
        throw "addArgsOnlyUndefined() failed with i = " + i + " returned value = " + value;
}

function testFallbackWithString() {
    var value = addArgsOnlyUndefined("foo", "bar");
    if (value !== "foobar")
        throw "addArgsOnlyUndefined(\"foo\", \"bar\") failed with i = " + i + " returned value = " + value;
}
testFallbackWithString();