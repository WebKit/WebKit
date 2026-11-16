function shouldThrowSyntaxError(source) {
    let error;
    try {
        new Function(source);
    } catch (e) {
        error = e;
    }
    if (!(error instanceof SyntaxError))
        throw new Error(`"${source}" should throw a SyntaxError`);
}

function shouldNotThrow(source) {
    new Function(source);
}

const words = ["implements", "interface", "let", "package", "private", "protected", "public", "static", "yield"];

for (const word of words) {
    const escaped = `\\u{${word.codePointAt(0).toString(16)}}${word.slice(1)}`;

    for (const name of [word, escaped]) {
        shouldThrowSyntaxError(`function f(${name}) { "use strict"; }`);
        shouldThrowSyntaxError(`function f(${name}) { 'use strict'; }`);
        shouldThrowSyntaxError(`function f(${name}) { "a"; "use strict"; }`);
        shouldThrowSyntaxError(`function f(a, ${name}) { "use strict"; }`);
        shouldThrowSyntaxError(`function f(${name}, a) { "use strict"; }`);
        shouldThrowSyntaxError(`(function (${name}) { "use strict"; })`);
        shouldThrowSyntaxError(`function* f(${name}) { "use strict"; }`);
        shouldThrowSyntaxError(`async function f(${name}) { "use strict"; }`);
        shouldThrowSyntaxError(`async function* f(${name}) { "use strict"; }`);
        shouldThrowSyntaxError(`(${name}) => { "use strict"; }`);
        shouldThrowSyntaxError(`${name} => { "use strict"; }`);
        shouldThrowSyntaxError(`(a, ${name}) => { "use strict"; }`);
        shouldThrowSyntaxError(`async (${name}) => { "use strict"; }`);
        shouldThrowSyntaxError(`async ${name} => { "use strict"; }`);
        shouldThrowSyntaxError(`({ m(${name}) { "use strict"; } })`);
        shouldThrowSyntaxError(`({ *m(${name}) { "use strict"; } })`);
        shouldThrowSyntaxError(`({ async m(${name}) { "use strict"; } })`);
        shouldThrowSyntaxError(`({ set x(${name}) { "use strict"; } })`);

        shouldThrowSyntaxError(`function ${name}() { "use strict"; }`);
        shouldThrowSyntaxError(`function ${name}() { "a"; "use strict"; }`);
        shouldThrowSyntaxError(`(function ${name}() { "use strict"; })`);
        shouldThrowSyntaxError(`function* ${name}() { "use strict"; }`);
        shouldThrowSyntaxError(`(function* ${name}() { "use strict"; })`);
        shouldThrowSyntaxError(`async function ${name}() { "use strict"; }`);
        shouldThrowSyntaxError(`(async function ${name}() { "use strict"; })`);
        shouldThrowSyntaxError(`async function* ${name}() { "use strict"; }`);
        shouldThrowSyntaxError(`(async function* ${name}() { "use strict"; })`);
        shouldThrowSyntaxError(`{ function ${name}() { "use strict"; } }`);

        shouldNotThrow(`function f(${name}) { }`);
        shouldNotThrow(`function f(${name}) { ("use strict"); }`);
        shouldNotThrow(`function f(${name}) { "use\\x20strict"; }`);
        shouldNotThrow(`function f(${name}) { function g() { "use strict"; } }`);
        shouldNotThrow(`function f(${name}) { return () => { "use strict"; }; }`);
        shouldNotThrow(`(${name}) => "use strict"`);
        shouldNotThrow(`function ${name}() { }`);
        shouldNotThrow(`(function ${name}() { })`);
        shouldNotThrow(`function ${name}() { function g() { "use strict"; } }`);
        shouldNotThrow(`({ ${name}() { "use strict"; } })`);
        shouldNotThrow(`({ get ${name}() { "use strict"; } })`);
        shouldNotThrow(`({ set ${name}(a) { "use strict"; } })`);
        shouldNotThrow(`({ ${name}: function () { "use strict"; } })`);
        shouldNotThrow(`(class { ${name}() { "use strict"; } })`);
        shouldNotThrow(`var ${name} = function () { "use strict"; };`);
        shouldNotThrow(`var ${name} = () => { "use strict"; };`);
    }

    let error;
    try {
        new Function(word, `"use strict";`);
    } catch (e) {
        error = e;
    }
    if (!(error instanceof SyntaxError))
        throw new Error(`new Function("${word}", '"use strict";') should throw a SyntaxError`);
    new Function(word, "");
}
