function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`FAIL: expected ${expected}, got ${actual}`);
}

function shouldNotInclude(string, substring) {
    if (string.includes(substring))
        throw new Error(`FAIL: ${JSON.stringify(string)} should not include ${JSON.stringify(substring)}`);
}

// When the subject of an iteration construct is a constant that does not itself
// emit expression info (e.g. a null literal), the [Symbol.iterator] get_by_id /
// op_spread should still be attributed to the iterating construct, not to the
// previous statement.

function check(prefix, body) {
    let source = `(${prefix} () {
"sentinelStatement";
${body}
})`;
    let f = eval(source);
    let error = null;
    let onError = e => { error = e; };
    try {
        let result = f();
        if (prefix === "function*")
            result.next();
        else if (prefix === "async function") {
            result.catch(onError);
            drainMicrotasks();
        }
    } catch (e) {
        onError(e);
    }
    if (!error)
        throw new Error(`FAIL: ${body} did not throw`);
    shouldBe(error.line, 3);
    shouldNotInclude(error.message, "sentinelStatement");
}

check("function", `for (const b of null) {}`);
check("function", `for (const b of undefined) {}`);
check("function", `const [a] = null;`);
check("function", `var a = [...null];`);
check("function", `var a = [1, ...null, 2];`);
check("function", `Array(...[...null]);`);
check("function", `new Array(...[...null]);`);
check("function*", `yield* null;`);
check("async function", `for await (const b of null) {}`);
