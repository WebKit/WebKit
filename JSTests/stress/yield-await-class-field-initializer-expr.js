// Tests for 'yield' and 'await' inside class field initializers, where the class is declared inside
// async and generator functions.
const verbose = false;

const wrappers = ['none', 'generator', 'async'];
const fieldModifiers = ['', 'static'];
const fieldInitExprs = ['yield', 'yield 42', 'await', 'await 3'];

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
        if (result != null) {
            print("Result: " + result);
        } else if (error != null) {
            print("Error: " + error);
        } else {
            print("Expecting either result or error!")
        }
    }
}
