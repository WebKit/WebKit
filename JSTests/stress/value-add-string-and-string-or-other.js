function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ' expected: ' + expected);
}

function add(a, b) { return a + b; }
noInline(add);
function addReversed(a, b) { return b + a; }
noInline(addReversed);
function template(a, b) { return `${a}-${b}`; }
noInline(template);

var inputs = ['x', undefined, 'yz', null, 'w'];
for (var i = 0; i < testLoopCount; ++i) {
    var value = inputs[i % inputs.length];
    shouldBe(add('s', value), 's' + String(value));
    shouldBe(addReversed('s', value), String(value) + 's');
    shouldBe(template('s', value), 's-' + String(value));
}

shouldBe(add('s', 1), 's1');
shouldBe(add('s', true), 'strue');
shouldBe(add('s', { toString() { return 'o'; } }), 'so');
shouldBe(add('s', [1, 2]), 's1,2');
