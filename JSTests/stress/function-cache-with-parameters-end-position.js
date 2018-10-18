function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

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

for (var i = 0; i < 10; ++i) {
    var f = Function('/*) {\n*/', 'return 42');
    shouldBe(f.toString(),
`function anonymous(/*) {
*/) {
return 42
}`);
}
shouldThrow(() => Function('/*', '*/){\nreturn 42'), `SyntaxError: Parameters should match arguments offered as parameters in Function constructor.`);

shouldThrow(() => Function('/*', '*/){\nreturn 43'), `SyntaxError: Parameters should match arguments offered as parameters in Function constructor.`);
for (var i = 0; i < 10; ++i) {
    var f = Function('/*) {\n*/', 'return 43');
    shouldBe(f.toString(),
`function anonymous(/*) {
*/) {
return 43
}`);
}

