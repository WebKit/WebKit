function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${expected} but got ${actual}`);
}

function shouldThrowSyntaxError(parse, source, message) {
    let thrown = false;
    let error;
    try {
        parse(source);
    } catch (e) {
        thrown = true;
        error = e;
    }
    if (!thrown)
        throw new Error(`"${source}" should throw "${message}" but did not throw`);
    // checkModuleSyntax() throws a string, so compare the string form.
    if (String(error) !== message)
        throw new Error(`"${source}" should throw "${message}" but threw "${String(error)}"`);
}

function shouldParse(parse, source) {
    try {
        parse(source);
    } catch (e) {
        throw new Error(`"${source}" should parse but threw "${String(e)}"`);
    }
}

const AsyncFunction = (async function () { }).constructor;
const AsyncGeneratorFunction = (async function* () { }).constructor;
const GeneratorFunction = (function* () { }).constructor;

const parseScript = (source) => (0, eval)(source);
const parseModule = (source) => checkModuleSyntax(source);
const parseWith = (constructor) => (source) => new constructor(source);

// Eval code does not inherit [Await] from an async caller. The async function runs
// synchronously up to here, so the error is rethrown to the caller rather than
// turned into a rejected promise.
function parseEvalCodeInAsyncFunction(source) {
    let error;
    (async function () {
        try {
            eval(source);
        } catch (e) {
            error = e;
        }
    })();
    if (error)
        throw error;
}

const parameters = [
    String.raw`await`,
    String.raw`\u0061wait`,
    String.raw`aw\u{61}it`,
    String.raw`[await]`,
    String.raw`[...await]`,
    String.raw`{ await }`,
    String.raw`{ x: \u0061wait }`,
];

// Each template is tested with BODY replaced by a try statement binding each parameter.
function sourcesFor(template) {
    let sources = [];
    for (let parameter of parameters)
        sources.push(template.replaceAll(`BODY`, `try { } catch (${parameter}) { }`));
    return sources;
}

function shouldReject(parse, template, reason, suffix = ``) {
    let message = `SyntaxError: Cannot use 'await' as a catch parameter name ${reason}.${suffix}`;
    for (let source of sourcesFor(template))
        shouldThrowSyntaxError(parse, source, message);
}

function shouldAccept(parse, template) {
    for (let source of sourcesFor(template))
        shouldParse(parse, source);
}

// 'await' is not an identifier in async functions.
shouldReject(parseScript, `async function f() { BODY }`, `in an async function`);
shouldReject(parseScript, `(async function () { BODY })`, `in an async function`);
shouldReject(parseScript, `(async () => { BODY })`, `in an async function`);
shouldReject(parseScript, `(async a => { BODY })`, `in an async function`);
shouldReject(parseScript, `({ async m() { BODY } })`, `in an async function`);
shouldReject(parseScript, `(class { static async m() { BODY } })`, `in an async function`);
shouldReject(parseScript, `async function* f() { BODY }`, `in an async function`);
shouldReject(parseScript, `({ async *m() { BODY } })`, `in an async function`);
shouldReject(parseScript, `"use strict"; async function f() { BODY }`, `in an async function`);
shouldReject(parseScript, `function f() { async function g() { BODY } }`, `in an async function`);
shouldReject(parseScript, `async function f() { { BODY } }`, `in an async function`);
shouldReject(parseScript, `async function f() { if (true) BODY }`, `in an async function`);
shouldReject(parseScript, `async function f() { switch (0) { case 0: BODY } }`, `in an async function`);
shouldReject(parseScript, `async function f() { try { BODY } finally { } }`, `in an async function`);
shouldReject(parseScript, `async function f() { try { } catch (e) { BODY } }`, `in an async function`);
shouldReject(parseScript, `async function f() { try { } finally { BODY } }`, `in an async function`);
shouldReject(parseScript, `async function f() { BODY finally { } }`, `in an async function`);
shouldReject(parseWith(AsyncFunction), `BODY`, `in an async function`);
shouldReject(parseWith(AsyncGeneratorFunction), `BODY`, `in an async function`);

// 'await' is not an identifier in class static blocks.
shouldReject(parseScript, `class C { static { BODY } }`, `in a static block`);
shouldReject(parseScript, `(class { static { BODY } })`, `in a static block`);
shouldReject(parseScript, `class C { static { { BODY } } }`, `in a static block`);
shouldReject(parseScript, `class C { static { try { } catch (e) { BODY } } }`, `in a static block`);
shouldReject(parseScript, `function f() { class C { static { BODY } } }`, `in a static block`);
shouldReject(parseScript, `class C { static { function f() { class D { static { BODY } } } } }`, `in a static block`);
shouldReject(parseScript, `class C { static { async function f() { BODY } } }`, `in an async function`);
shouldReject(parseScript, `class C { static { (async () => { BODY }); } }`, `in an async function`);

// 'await' is not an identifier anywhere in module code.
shouldReject(parseModule, `BODY`, `in a module`, `:1`);
shouldReject(parseModule, `{ BODY }`, `in a module`, `:1`);
shouldReject(parseModule, `function f() { BODY }`, `in a module`, `:1`);
shouldReject(parseModule, `function* f() { BODY }`, `in a module`, `:1`);
shouldReject(parseModule, `(() => { BODY });`, `in a module`, `:1`);
shouldReject(parseModule, `({ m() { BODY } });`, `in a module`, `:1`);
shouldReject(parseModule, `export default function () { BODY }`, `in a module`, `:1`);
shouldReject(parseModule, `async function f() { BODY }`, `in an async function`, `:1`);
shouldReject(parseModule, `class C { static { BODY } }`, `in a static block`, `:1`);

// 'await' is an identifier everywhere else.
shouldAccept(parseScript, `BODY`);
shouldAccept(parseScript, `"use strict"; BODY`);
shouldAccept(parseScript, `try { } catch (e) { BODY } finally { BODY }`);
shouldAccept(parseScript, `function f() { BODY }`);
shouldAccept(parseScript, `function f() { "use strict"; BODY }`);
shouldAccept(parseScript, `function* f() { BODY }`);
shouldAccept(parseScript, `(() => { BODY })`);
shouldAccept(parseScript, `({ m() { BODY } })`);
shouldAccept(parseScript, `({ get x() { BODY } })`);
shouldAccept(parseScript, `(class { constructor() { BODY } })`);
shouldAccept(parseScript, `(class { static m() { BODY } })`);
shouldAccept(parseScript, `async function f() { function g() { BODY } }`);
shouldAccept(parseScript, `async function f() { function* g() { BODY } }`);
shouldAccept(parseScript, `async function f() { (() => { BODY }); }`);
shouldAccept(parseScript, `async function f() { ({ m() { BODY } }); }`);
shouldAccept(parseScript, `(async () => { (() => { BODY }); })`);
shouldAccept(parseScript, `async function* f() { function g() { BODY } }`);
shouldAccept(parseScript, `class C { static { function f() { BODY } } }`);
shouldAccept(parseScript, `class C { static { (() => { BODY }); } }`);
shouldAccept(parseScript, `class C { static { ({ m() { BODY } }); } }`);
shouldAccept(parseScript, `class C { static { } m() { BODY } }`);
shouldAccept(parseScript, `class C { static { } } BODY`);
shouldAccept(parseScript, `async function f() { } BODY`);
shouldAccept(parseWith(Function), `BODY`);
shouldAccept(parseWith(GeneratorFunction), `BODY`);
shouldAccept((source) => eval(source), `BODY`);
shouldAccept(parseEvalCodeInAsyncFunction, `BODY`);

// Other catch parameters are not affected.
for (let parameter of [`e`, `async`, `yield`, `let`, `awaits`, `{ await: e }`, `[e = async () => { await 0; }]`]) {
    let statement = `try { } catch (${parameter}) { }`;
    shouldParse(parseScript, `async function f() { ${statement} }`);
    shouldParse(parseScript, `(async () => { ${statement} })`);
    shouldParse(parseWith(AsyncFunction), statement);
}
for (let parameter of [`e`, `async`, `awaits`, `{ await: e }`, `[e = async () => { await 0; }]`]) {
    let statement = `try { } catch (${parameter}) { }`;
    shouldParse(parseScript, `class C { static { ${statement} } }`);
    shouldParse(parseModule, statement);
}
shouldParse(parseScript, `async function f() { try { } catch { await 0; } }`);
shouldParse(parseScript, `async function f() { try { } catch (e) { await e; } }`);
shouldParse(parseScript, `async function f() { try { } catch ([e = await 0]) { } }`);
shouldParse(parseModule, `try { } catch { await 0; }`);
shouldParse(parseModule, `try { } catch (e) { await e; }`);

// 'yield' and 'let' catch parameters keep their own errors.
shouldThrowSyntaxError(parseScript, `function* f() { try { } catch (yield) { } }`, `SyntaxError: Cannot use 'yield' as a catch parameter name in a generator function.`);
shouldThrowSyntaxError(parseScript, `async function* f() { try { } catch (yield) { } }`, `SyntaxError: Cannot use 'yield' as a catch parameter name in a generator function.`);
shouldThrowSyntaxError(parseScript, `"use strict"; try { } catch (yield) { }`, `SyntaxError: Cannot use 'yield' as a catch parameter name in strict mode.`);
shouldThrowSyntaxError(parseScript, `"use strict"; try { } catch (let) { }`, `SyntaxError: Cannot use 'let' as a catch parameter name in strict mode.`);
shouldThrowSyntaxError(parseScript, `class C { static { try { } catch (yield) { } } }`, `SyntaxError: Cannot use 'yield' as a catch parameter name in strict mode.`);

// A catch parameter named 'await' is an ordinary binding where it is allowed.
try {
    throw 1;
} catch (await) {
    shouldBe(await, 1);
    await = 2;
    shouldBe(await, 2);
    shouldBe((() => await)(), 2);
}

try {
    throw 3;
} catch (\u0061wait) {
    shouldBe(await, 3);
}

try {
    throw { await: 4 };
} catch ({ await }) {
    shouldBe(await, 4);
}

function inFunction(value) {
    "use strict";
    try {
        throw value;
    } catch (await) {
        return await;
    }
}
shouldBe(inFunction(5), 5);

async function inAsyncFunction(value) {
    let nested = () => {
        try {
            throw value;
        } catch (await) {
            return await + 1;
        }
    };
    return await nested();
}

class InStaticBlock {
    static value;
    static {
        this.value = function () {
            try {
                throw 8;
            } catch (await) {
                return await;
            }
        }();
    }
}
shouldBe(InStaticBlock.value, 8);

let result;
inAsyncFunction(6).then((value) => { result = value; });
drainMicrotasks();
shouldBe(result, 7);
