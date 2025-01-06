// Tests for 'yield' and 'await' inside class field initializers, where the class is declared inside
// async and generator functions.
const verbose = false;

const wrappers = ['none', 'generator', 'async'];
const fieldModifiers = ['', 'static'];
const fieldInitExprs = ['yield', 'yield 42', 'function() { yield 21; }', 'await', 'await 3', '() => await 7', 'function() { await 9; }'];

function genTestCases() {
    let cases = [];
    for (const wrapper of wrappers) {
        for (const fieldModifier of fieldModifiers) {
            for (const fieldInitExpr of fieldInitExprs) {
                cases.push({ wrapper, fieldModifier, fieldInitExpr });
            }
        }
    }
    return cases;
}

function genTestScript(c) {
    let tabs = 0;
    let script = "";

    function append(line) {
        for (t = 0; t < tabs; t++)
            script += '  ';
        script += line + '\n';
    }

    switch (c.wrapper) {
        case 'generator':
            append('function * g() {');
            break;
        case 'async':
            append('async function f() {');
            break;
        case 'none':
            break;
    }
    tabs++;
    append('class C {');
    tabs++;
    append(`${c.fieldModifier} f = ${c.fieldInitExpr};`);
    tabs--;
    append('}');
    tabs--;
    if (c.wrapper !== 'none') append('}');
    return script;
}

function expected(c, result, error) {
    if (c.fieldInitExpr === 'await') {
        // 'await' will parse as an identifier.
        if (c.wrapper === 'none' && c.fieldModifier === 'static') {
            // In this case, 'await' as identifier produces a ReferenceError.
            return result === null && error instanceof ReferenceError;
        }
        // In these cases, 'await' as identifier has value 'undefined').
        return result === undefined && error === null;
    }
    // All other cases should result in a SyntaxError.
    return result === null && error instanceof SyntaxError;
}

// Verify that 'await' and 'yield' do not parse as keywords (directly) inside class field initializers.
function testNegativeCases() {
    cases = genTestCases();

    for (const c of cases) {
        let script = genTestScript(c);
        let result = null;
        let error = null;
        try {
            result = eval(script);
        } catch (e) {
            error = e;
        }

        if (verbose || !expected(c, result, error)) {
            print(`Case: ${c.wrapper}:${c.fieldModifier}:${c.fieldInitExpr}`);
            print(`Script:\n${script}`);
            print(`Result: ${result}`);
            print(`Error: ${error}\n`);
        }
    }
}

// Verify that 'await' and 'yield' work inside anonymous async / generator functions
// used as class field initializers.
function testPositiveCases() {
    function assertEq(got, expected) {
        if (got !== expected) {
            throw Error(`Got: ${got}, Expected: ${expected}`);
        }
    }

    class C {
        asyncFn0 = async y => await y;
        asyncFn1 = async () => { return await 5 };
        asyncFn2 = async function(x) { return await x; };

        yieldFn0 = function* () { yield 9; };
        yieldFn1 = async function* () { yield await 11; };
    };
    let c = new C();

    c.asyncFn0(3).then(x => assertEq(x, 3));
    c.asyncFn1().then(x => assertEq(x, 5));
    c.asyncFn2(7).then(x => assertEq(x, 7));

    assertEq(c.yieldFn0().next().value, 9);
    c.yieldFn1().next().then(x => assertEq(x.value, 11));
}

testNegativeCases();
testPositiveCases();
