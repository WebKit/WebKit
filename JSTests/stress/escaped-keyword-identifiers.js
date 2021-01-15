function shouldNotThrow(func) {
    func();
}

function shouldThrowSyntaxError(script) {
    let error;
    try {
        eval(script);
    } catch (e) {
        error = e;
    }

    if (!(error instanceof SyntaxError))
        throw new Error('Expected SyntaxError!');
}

shouldNotThrow(() => { l\u0065t: 3; });
shouldNotThrow(() => { aw\u0061it: 3; });
shouldNotThrow(() => { yi\u0065ld: 3; });
shouldNotThrow(() => { st\u0061tic: 3; });
shouldThrowSyntaxError('nu\\u006cl: 3;');
shouldThrowSyntaxError('async function f() { aw\\u0061it: 3; }');
shouldThrowSyntaxError('function* g() { yi\\u0065ld: 3; }');

shouldNotThrow(() => { var l\u0065t = 3; });
shouldNotThrow(() => { var aw\u0061it = 3; });
shouldNotThrow(() => { var yi\u0065ld = 3; });
shouldNotThrow(() => { var st\u0061tic = 3; });
shouldThrowSyntaxError('var nu\\u006cl = 3;');
shouldThrowSyntaxError('async function f() { var aw\\u0061it = 3; }');
shouldThrowSyntaxError('function* g() { var yi\\u0065ld = 3; }');

shouldNotThrow(() => { let aw\u0061it = 3; });
shouldNotThrow(() => { let yi\u0065ld = 3; });
shouldNotThrow(() => { let st\u0061tic = 3; });
shouldThrowSyntaxError('let l\\u0065t = 3;');
shouldThrowSyntaxError('let nu\\u006cl = 3;');
shouldThrowSyntaxError('async function f() { let aw\\u0061it = 3; }');
shouldThrowSyntaxError('function* g() { let yi\\u0065ld = 3; }');

shouldNotThrow(() => { const aw\u0061it = 3; });
shouldNotThrow(() => { const yi\u0065ld = 3; });
shouldNotThrow(() => { const st\u0061tic = 3; });
shouldThrowSyntaxError('const l\\u0065t = 3;');
shouldThrowSyntaxError('const nu\\u006cl = 3;');
shouldThrowSyntaxError('async function f() { const aw\\u0061it = 3; }');
shouldThrowSyntaxError('function* g() { const yi\\u0065ld = 3; }');

shouldNotThrow(() => { class aw\u0061it {} });
shouldThrowSyntaxError('class l\\u0065t {}');
shouldThrowSyntaxError('class yi\\u0065ld {}');
shouldThrowSyntaxError('class st\\u0061tic {}');
shouldThrowSyntaxError('class nu\\u006cl {}');
shouldThrowSyntaxError('async function f() { class aw\\u0061it {} }');
shouldThrowSyntaxError('function* g() { class yi\\u0065ld {} }');

shouldNotThrow(() => { async function aw\u0061it() {} });
shouldNotThrow(() => { function* yi\u0065ld() {} });
shouldThrowSyntaxError('async function f() { function aw\\u0061it() {} }');
shouldThrowSyntaxError('function* g() { function yi\\u0065ld() {} }');

shouldNotThrow(() => { function f(aw\u0061it) {} });
shouldNotThrow(() => { function g(yi\u0065ld) {} });
shouldThrowSyntaxError('async function f(aw\\u0061it) {}');
shouldThrowSyntaxError('function* g(yi\\u0065ld) {}');

shouldNotThrow(() => { function l\u0065t() {} });
shouldNotThrow(() => { function st\u0061tic() {} });
shouldNotThrow(() => { function f(l\u0065t) {} });
shouldNotThrow(() => { function f(st\u0061tic) {} });
shouldThrowSyntaxError('function f() { function nu\\u006cl() {} }');
shouldThrowSyntaxError('function f(nu\\u006cl) {}');

shouldNotThrow(() => { l\u0065t => 3; });
shouldNotThrow(() => { aw\u0061it => 3; });
shouldNotThrow(() => { yi\u0065ld => 3; });
shouldNotThrow(() => { st\u0061tic => 3; });
shouldThrowSyntaxError('nu\\u006cl => 3;');
shouldThrowSyntaxError('async function f() { aw\\u0061it => 3; }');
shouldThrowSyntaxError('function* g() { yi\\u0065ld => 3; }');
