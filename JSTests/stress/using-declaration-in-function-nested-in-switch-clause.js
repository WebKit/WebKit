//@ requireOptions("--useExplicitResourceManagement=true")

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`Expected ${expected} but got ${actual}`);
}

function shouldParse(source) {
    try {
        (0, eval)(source);
    } catch (error) {
        throw new Error(`Expected no error but got ${error}: ${source}`);
    }
}

function shouldThrowSyntaxError(source) {
    let error;
    try {
        (0, eval)(source);
    } catch (e) {
        error = e;
    }
    if (!(error instanceof SyntaxError))
        throw new Error(`Expected SyntaxError but got ${error}: ${source}`);
}

function switches(statements) {
    return [
        `switch (0) { case 0: ${statements} }`,
        `switch (0) { default: ${statements} }`,
        `switch (0) { case 1: break; case 0: ${statements} }`,
        `switch (0) { default: break; case 0: ${statements} }`,
    ];
}

function clauses(statements) {
    return [
        ...switches(statements),
        `function outer() { switch (0) { case 0: ${statements} } }`,
        `async function outer() { switch (0) { default: ${statements} } }`,
    ];
}

// The restriction only applies to declarations directly in the StatementList of a clause.
// A function nested in the clause has its own body.
let nested = [
    `function f() { using x = null; }`,
    `function* f() { using x = null; }`,
    `async function f() { using x = null; }`,
    `async function* f() { using x = null; }`,
    `(function () { using x = null; });`,
    `(() => { using x = null; });`,
    `(() => () => { using x = null; });`,
    `({ f() { using x = null; } });`,
    `({ get f() { using x = null; } });`,
    `({ set f(value) { using x = null; } });`,
    `class C { constructor() { using x = null; } }`,
    `class C { f() { using x = null; } }`,
    `class C { static f() { using x = null; } }`,
    `class C { #f() { using x = null; } }`,
    `class C { f = () => { using x = null; }; }`,
    `class C { static { using x = null; } }`,
    `function f(g = () => { using x = null; }) { }`,
    `function f() { if (true) { using x = null; } }`,
    `function f() { for (using x of []) { } }`,
    `async function f() { await using x = null; }`,
    `async function* f() { await using x = null; }`,
    `(async function () { await using x = null; });`,
    `(async () => { await using x = null; });`,
    `({ async f() { await using x = null; } });`,
    `class C { async f() { await using x = null; } }`,
    `class C { static async f() { await using x = null; } }`,
    `class C { f = async () => { await using x = null; }; }`,
    `function f(g = async () => { await using x = null; }) { }`,
];

for (let source of nested) {
    for (let clause of clauses(source))
        shouldParse(clause);
}

// These were already accepted. A block or a for statement has its own scope.
for (let clause of clauses(`{ using x = null; }`))
    shouldParse(clause);
for (let clause of clauses(`for (using x of []) { }`))
    shouldParse(clause);
for (let clause of clauses(`try { using x = null; } catch { using y = null; } finally { using z = null; }`))
    shouldParse(clause);
for (let clause of switches(`{ await using x = null; }`)) {
    shouldParse(`async function outer() { ${clause} }`);
    shouldParse(`async () => { ${clause} }`);
}

// A switch statement does not affect the declarations following it.
shouldParse(`{ switch (0) { case 0: break; } using x = null; }`);
shouldParse(`function f() { switch (0) { default: } using x = null; }`);
shouldParse(`async function f() { switch (0) { case 0: break; } await using x = null; }`);
shouldParse(`switch (0) { case 0: { switch (1) { default: } using x = null; } }`);

// Still disallowed directly in a clause.
let direct = [
    `using x = null;`,
    `; using x = null;`,
    `break; using x = null;`,
    `{ using y = null; } using x = null;`,
    `function f() { using y = null; } using x = null;`,
    `(() => { using y = null; })(); using x = null;`,
    `({ f() { using y = null; } }); using x = null;`,
    `class C { f() { using y = null; } static { using z = null; } } using x = null;`,
    `for (using y of []) { } using x = null;`,
    `switch (1) { case 1: { using y = null; } } using x = null;`,
];

for (let source of direct) {
    for (let clause of clauses(source))
        shouldThrowSyntaxError(clause);
    for (let clause of switches(source.replace(`using x`, `await using x`))) {
        shouldThrowSyntaxError(`async function outer() { ${clause} }`);
        shouldThrowSyntaxError(`async () => { ${clause} }`);
    }
}

// Including in a switch statement inside a function nested in a clause.
for (let clause of clauses(`function f() { switch (1) { case 1: using x = null; } }`))
    shouldThrowSyntaxError(clause);
for (let clause of clauses(`(() => { switch (1) { default: using x = null; } });`))
    shouldThrowSyntaxError(clause);
for (let clause of clauses(`async function f() { switch (1) { case 1: await using x = null; } }`))
    shouldThrowSyntaxError(clause);
for (let clause of clauses(`{ switch (1) { case 1: using x = null; } }`))
    shouldThrowSyntaxError(clause);

{
    let log = [];
    switch (0) {
    case 0:
        function f() {
            using x = { [Symbol.dispose]() { log.push("dispose f"); } };
            log.push("f");
        }
        let g = () => {
            using x = { [Symbol.dispose]() { log.push("dispose g"); } };
            log.push("g");
        };
        f();
        g();
        log.push("done");
    }
    shouldBe(log.join(), "f,dispose f,g,dispose g,done");
}

{
    let log = [];
    async function test(value) {
        switch (value) {
        case 0:
            log.push("unreachable");
        default:
            async function f() {
                await using x = { [Symbol.asyncDispose]() { log.push("dispose f"); } };
                log.push("f");
            }
            let g = async () => {
                await using x = { [Symbol.dispose]() { log.push("dispose g"); } };
                log.push("g");
            };
            await f();
            await g();
            log.push("done");
        }
    }
    let rejected = false;
    test(1).catch(() => { rejected = true; });
    drainMicrotasks();
    shouldBe(rejected, false);
    shouldBe(log.join(), "f,dispose f,g,dispose g,done");
}
