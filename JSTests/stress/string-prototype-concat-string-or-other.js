function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ' expected: ' + expected);
}

function shouldThrow(func, errorType) {
    var error = null;
    try {
        func();
    } catch (e) {
        error = e;
    }
    if (!(error instanceof errorType))
        throw new Error('bad error: ' + error);
}

function concat1(a, b) { return a.concat(b); }
noInline(concat1);
function concat2(a, b, c) { return a.concat(b, c); }
noInline(concat2);
function concatThis(thisValue, a) { return String.prototype.concat.call(thisValue, a); }
noInline(concatThis);

var inputs = ['x', undefined, 'yz', null, 'w'];
for (var i = 0; i < testLoopCount; ++i) {
    var value = inputs[i % inputs.length];
    var other = inputs[(i + 1) % inputs.length];
    shouldBe(concat1('s', value), 's' + String(value));
    shouldBe(concat2('s', value, other), 's' + String(value) + String(other));
    shouldBe(concat2('s', 't', value), 'st' + String(value));
    if (typeof value === 'string')
        shouldBe(concatThis(value, 's'), value + 's');
    else
        shouldThrow(() => concatThis(value, 's'), TypeError);
}

shouldThrow(() => concatThis(undefined, 's'), TypeError);
shouldThrow(() => concatThis(null, 's'), TypeError);
shouldBe(concat1('s', 1), 's1');
shouldBe(concat1('s', { toString() { return 'o'; } }), 'so');
shouldBe(concatThis(42, 's'), '42s');
