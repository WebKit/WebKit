function assert(cond, msg = "") {
    if (!cond)
        throw new Error(msg);
}
noInline(assert);

function shouldThrowSyntaxError(str, message) {
    var hadError = false;
    try {
        eval(str);
    } catch (e) {
        if (e instanceof SyntaxError) {
            hadError = true;
            if (typeof message === "string")
                assert(e.message === message, "Expected '" + message + "' but threw '" + e.message + "'");
        }
    }
    assert(hadError, "Did not throw syntax error");
}
noInline(shouldThrowSyntaxError);

var AsyncFunction = (async function() {}).constructor;

// AsyncFunctionExpression
shouldThrowSyntaxError("(async function() { var await; })", "Cannot use 'await' as a variable name in an async function.");
shouldThrowSyntaxError("(async function() { var [await] = []; })", "Cannot use 'await' as a variable name in an async function.");
shouldThrowSyntaxError("(async function() { var [...await] = []; })", "Cannot use 'await' as a variable name in an async function.");
shouldThrowSyntaxError("(async function() { var {await} = {}; })", "Cannot use 'await' as a variable name in an async function.");
shouldThrowSyntaxError("(async function() { var {isAsync: await} = {}; })", "Cannot use 'await' as a variable name in an async function.");
shouldThrowSyntaxError("(async function() { var {isAsync: await} = {}; })", "Cannot use 'await' as a variable name in an async function.");
shouldThrowSyntaxError("(async function() { let await; })", "Cannot use 'await' as a lexical variable name in an async function.");
shouldThrowSyntaxError("(async function() { let [await] = []; })", "Cannot use 'await' as a lexical variable name in an async function.");
shouldThrowSyntaxError("(async function() { let [...await] = []; })", "Cannot use 'await' as a lexical variable name in an async function.");
shouldThrowSyntaxError("(async function() { let {await} = {}; })", "Cannot use 'await' as a lexical variable name in an async function.");
shouldThrowSyntaxError("(async function() { let {isAsync: await} = {}; })", "Cannot use 'await' as a lexical variable name in an async function.");
shouldThrowSyntaxError("(async function() { let {isAsync: await} = {}; })", "Cannot use 'await' as a lexical variable name in an async function.");
shouldThrowSyntaxError("(async function() { const await; })", "Cannot use 'await' as a lexical variable name in an async function.");
shouldThrowSyntaxError("(async function() { const [await] = []; })", "Cannot use 'await' as a lexical variable name in an async function.");
shouldThrowSyntaxError("(async function() { const [...await] = []; })", "Cannot use 'await' as a lexical variable name in an async function.");
shouldThrowSyntaxError("(async function() { const {await} = {}; })", "Cannot use 'await' as a lexical variable name in an async function.");
shouldThrowSyntaxError("(async function() { const {isAsync: await} = {}; })", "Cannot use 'await' as a lexical variable name in an async function.");
shouldThrowSyntaxError("(async function() { const {isAsync: await} = {}; })", "Cannot use 'await' as a lexical variable name in an async function.");
shouldThrowSyntaxError("(async function() { function await() {} })", "Cannot declare function named 'await' in an async function.");
shouldThrowSyntaxError("(async function() { async function await() {} })", "Cannot declare function named 'await' in an async function.");
shouldThrowSyntaxError("(async function(await) {})", "Cannot use 'await' as a parameter name in an async function.");
shouldThrowSyntaxError("(async function f([await]) {})", "Cannot use 'await' as a parameter name in an async function.");
shouldThrowSyntaxError("(async function f([...await]) {})", "Cannot use 'await' as a parameter name in an async function.");
shouldThrowSyntaxError("(async function f(...await) {})", "Cannot use 'await' as a parameter name in an async function.");
shouldThrowSyntaxError("(async function f({await}) {})", "Cannot use 'await' as a parameter name in an async function.");
shouldThrowSyntaxError("(async function f({isAsync: await}) {})", "Cannot use 'await' as a parameter name in an async function.");

// AsyncFunctionDeclaration
shouldThrowSyntaxError("async function f() { var await; }", "Cannot use 'await' as a variable name in an async function.");
shouldThrowSyntaxError("async function f() { var [await] = []; })", "Cannot use 'await' as a variable name in an async function.");
shouldThrowSyntaxError("async function f() { var [...await] = []; })", "Cannot use 'await' as a variable name in an async function.");
shouldThrowSyntaxError("async function f() { var {await} = {}; })", "Cannot use 'await' as a variable name in an async function.");
shouldThrowSyntaxError("async function f() { var {isAsync: await} = {}; })", "Cannot use 'await' as a variable name in an async function.");
shouldThrowSyntaxError("async function f() { var {isAsync: await} = {}; })", "Cannot use 'await' as a variable name in an async function.");
shouldThrowSyntaxError("async function f() { let await; }", "Cannot use 'await' as a lexical variable name in an async function.");
shouldThrowSyntaxError("async function f() { let [await] = []; }", "Cannot use 'await' as a lexical variable name in an async function.");
shouldThrowSyntaxError("async function f() { let [...await] = []; }", "Cannot use 'await' as a lexical variable name in an async function.");
shouldThrowSyntaxError("async function f() { let {await} = {}; }", "Cannot use 'await' as a lexical variable name in an async function.");
shouldThrowSyntaxError("async function f() { let {isAsync: await} = {}; }", "Cannot use 'await' as a lexical variable name in an async function.");
shouldThrowSyntaxError("async function f() { let {isAsync: await} = {}; }", "Cannot use 'await' as a lexical variable name in an async function.");
shouldThrowSyntaxError("async function f() { const await; }", "Cannot use 'await' as a lexical variable name in an async function.");
shouldThrowSyntaxError("async function f() { const [await] = []; }", "Cannot use 'await' as a lexical variable name in an async function.");
shouldThrowSyntaxError("async function f() { const [...await] = []; }", "Cannot use 'await' as a lexical variable name in an async function.");
shouldThrowSyntaxError("async function f() { const {await} = {}; }", "Cannot use 'await' as a lexical variable name in an async function.");
shouldThrowSyntaxError("async function f() { const {isAsync: await} = {}; }", "Cannot use 'await' as a lexical variable name in an async function.");
shouldThrowSyntaxError("async function f() { const {isAsync: await} = {}; }", "Cannot use 'await' as a lexical variable name in an async function.");
shouldThrowSyntaxError("async function f() { function await() {} }", "Cannot declare function named 'await' in an async function.");
shouldThrowSyntaxError("async function f() { async function await() {} }", "Cannot declare function named 'await' in an async function.");
shouldThrowSyntaxError("async function f(await) {}", "Cannot use 'await' as a parameter name in an async function.");
shouldThrowSyntaxError("async function f([await]) {}", "Cannot use 'await' as a parameter name in an async function.");
shouldThrowSyntaxError("async function f([...await]) {}", "Cannot use 'await' as a parameter name in an async function.");
shouldThrowSyntaxError("async function f(...await) {}", "Cannot use 'await' as a parameter name in an async function.");
shouldThrowSyntaxError("async function f({await}) {}", "Cannot use 'await' as a parameter name in an async function.");
shouldThrowSyntaxError("async function f({isAsync: await}) {}", "Cannot use 'await' as a parameter name in an async function.");

// AsyncArrowFunction
shouldThrowSyntaxError("var f = async () => { var await; }", "Cannot use 'await' as a variable name in an async function.");
shouldThrowSyntaxError("var f = async () => { var [await] = []; })", "Cannot use 'await' as a variable name in an async function.");
shouldThrowSyntaxError("var f = async () => { var [...await] = []; })", "Cannot use 'await' as a variable name in an async function.");
shouldThrowSyntaxError("var f = async () => { var {await} = {}; })", "Cannot use 'await' as a variable name in an async function.");
shouldThrowSyntaxError("var f = async () => { var {isAsync: await} = {}; })", "Cannot use 'await' as a variable name in an async function.");
shouldThrowSyntaxError("var f = async () => { var {isAsync: await} = {}; })", "Cannot use 'await' as a variable name in an async function.");
shouldThrowSyntaxError("var f = async () => { let await; }", "Cannot use 'await' as a lexical variable name in an async function.");
shouldThrowSyntaxError("var f = async () => { let [await] = []; }", "Cannot use 'await' as a lexical variable name in an async function.");
shouldThrowSyntaxError("var f = async () => { let [...await] = []; }", "Cannot use 'await' as a lexical variable name in an async function.");
shouldThrowSyntaxError("var f = async () => { let {await} = {}; }", "Cannot use 'await' as a lexical variable name in an async function.");
shouldThrowSyntaxError("var f = async () => { let {isAsync: await} = {}; }", "Cannot use 'await' as a lexical variable name in an async function.");
shouldThrowSyntaxError("var f = async () => { let {isAsync: await} = {}; }", "Cannot use 'await' as a lexical variable name in an async function.");
shouldThrowSyntaxError("var f = async () => { const await; }", "Cannot use 'await' as a lexical variable name in an async function.");
shouldThrowSyntaxError("var f = async () => { const [await] = []; }", "Cannot use 'await' as a lexical variable name in an async function.");
shouldThrowSyntaxError("var f = async () => { const [...await] = []; }", "Cannot use 'await' as a lexical variable name in an async function.");
shouldThrowSyntaxError("var f = async () => { const {await} = {}; }", "Cannot use 'await' as a lexical variable name in an async function.");
shouldThrowSyntaxError("var f = async () => { const {isAsync: await} = {}; }", "Cannot use 'await' as a lexical variable name in an async function.");
shouldThrowSyntaxError("var f = async () => { const {isAsync: await} = {}; }", "Cannot use 'await' as a lexical variable name in an async function.");
shouldThrowSyntaxError("var f = async () => { function await() {} }", "Cannot declare function named 'await' in an async function.");
shouldThrowSyntaxError("var f = async () => { async function await() {} }", "Cannot declare function named 'await' in an async function.");
shouldThrowSyntaxError("var f = async (await) => {}", "Cannot use 'await' as a parameter name in an async function.");
shouldThrowSyntaxError("var f = async ([await]) => {}", "Cannot use 'await' as a parameter name in an async function.");
shouldThrowSyntaxError("var f = async ([...await]) => {}", "Cannot use 'await' as a parameter name in an async function.");
shouldThrowSyntaxError("var f = async (...await) => {}", "Cannot use 'await' as a parameter name in an async function.");
shouldThrowSyntaxError("var f = async ({await}) => {}", "Cannot use 'await' as a parameter name in an async function.");
shouldThrowSyntaxError("var f = async ({isAsync: await}) => {}", "Cannot use 'await' as a parameter name in an async function.");
shouldThrowSyntaxError("var f = async await => {}", "Cannot use 'await' as a parameter name in an async function.");

// AsyncMethod
shouldThrowSyntaxError("var O = { async f() { var await; } }", "Cannot use 'await' as a variable name in an async function.");
shouldThrowSyntaxError("var O = { async f() { var [await] = []; } }", "Cannot use 'await' as a variable name in an async function.");
shouldThrowSyntaxError("var O = { async f() { var [...await] = []; } }", "Cannot use 'await' as a variable name in an async function.");
shouldThrowSyntaxError("var O = { async f() { var {await} = {}; } }", "Cannot use 'await' as a variable name in an async function.");
shouldThrowSyntaxError("var O = { async f() { var {isAsync: await} = {}; } }", "Cannot use 'await' as a variable name in an async function.");
shouldThrowSyntaxError("var O = { async f() { var {isAsync: await} = {}; } }", "Cannot use 'await' as a variable name in an async function.");
shouldThrowSyntaxError("var O = { async f() { let await; } }", "Cannot use 'await' as a lexical variable name in an async function.");
shouldThrowSyntaxError("var O = { async f() { let [await] = []; } }", "Cannot use 'await' as a lexical variable name in an async function.");
shouldThrowSyntaxError("var O = { async f() { let [...await] = []; } }", "Cannot use 'await' as a lexical variable name in an async function.");
shouldThrowSyntaxError("var O = { async f() { let {await} = {}; } }", "Cannot use 'await' as a lexical variable name in an async function.");
shouldThrowSyntaxError("var O = { async f() { let {isAsync: await} = {}; } }", "Cannot use 'await' as a lexical variable name in an async function.");
shouldThrowSyntaxError("var O = { async f() { let {isAsync: await} = {}; } }", "Cannot use 'await' as a lexical variable name in an async function.");
shouldThrowSyntaxError("var O = { async f() { const await; } }", "Cannot use 'await' as a lexical variable name in an async function.");
shouldThrowSyntaxError("var O = { async f() { const [await] = []; } }", "Cannot use 'await' as a lexical variable name in an async function.");
shouldThrowSyntaxError("var O = { async f() { const [...await] = []; } }", "Cannot use 'await' as a lexical variable name in an async function.");
shouldThrowSyntaxError("var O = { async f() { const {await} = {}; } }", "Cannot use 'await' as a lexical variable name in an async function.");
shouldThrowSyntaxError("var O = { async f() { const {isAsync: await} = {}; } }", "Cannot use 'await' as a lexical variable name in an async function.");
shouldThrowSyntaxError("var O = { async f() { const {isAsync: await} = {}; } }", "Cannot use 'await' as a lexical variable name in an async function.");
shouldThrowSyntaxError("var O = { async f() { function await() {} }", "Cannot declare function named 'await' in an async function.");
shouldThrowSyntaxError("var O = { async f() { async function await() {} } }", "Cannot declare function named 'await' in an async function.");
shouldThrowSyntaxError("var O = { async f(await) {} } ", "Cannot use 'await' as a parameter name in an async function.");
shouldThrowSyntaxError("var O = { async f([await]) {}", "Cannot use 'await' as a parameter name in an async function.");
shouldThrowSyntaxError("var O = { async f([...await]) {}", "Cannot use 'await' as a parameter name in an async function.");
shouldThrowSyntaxError("var O = { async f(...await) {}", "Cannot use 'await' as a parameter name in an async function.");
shouldThrowSyntaxError("var O = { async f({await}) {}", "Cannot use 'await' as a parameter name in an async function.");
shouldThrowSyntaxError("var O = { async f({isAsync: await}) {}", "Cannot use 'await' as a parameter name in an async function.");

// AsyncFunction constructor
shouldThrowSyntaxError("AsyncFunction('var await;')", "Cannot use 'await' as a variable name in an async function.");
shouldThrowSyntaxError("AsyncFunction('var [await] = [];')", "Cannot use 'await' as a variable name in an async function.");
shouldThrowSyntaxError("AsyncFunction('var [...await] = [];')", "Cannot use 'await' as a variable name in an async function.");
shouldThrowSyntaxError("AsyncFunction('var {await} = {};')", "Cannot use 'await' as a variable name in an async function.");
shouldThrowSyntaxError("AsyncFunction('var {isAsync: await} = {};')", "Cannot use 'await' as a variable name in an async function.");
shouldThrowSyntaxError("AsyncFunction('var {isAsync: await} = {};')", "Cannot use 'await' as a variable name in an async function.");
shouldThrowSyntaxError("AsyncFunction('let await;')", "Cannot use 'await' as a lexical variable name in an async function.");
shouldThrowSyntaxError("AsyncFunction('let [await] = [];')", "Cannot use 'await' as a lexical variable name in an async function.");
shouldThrowSyntaxError("AsyncFunction('let [...await] = [];')", "Cannot use 'await' as a lexical variable name in an async function.");
shouldThrowSyntaxError("AsyncFunction('let {await} = {};')", "Cannot use 'await' as a lexical variable name in an async function.");
shouldThrowSyntaxError("AsyncFunction('let {isAsync: await} = {};')", "Cannot use 'await' as a lexical variable name in an async function.");
shouldThrowSyntaxError("AsyncFunction('let {isAsync: await} = {};')", "Cannot use 'await' as a lexical variable name in an async function.");
shouldThrowSyntaxError("AsyncFunction('const await;')", "Cannot use 'await' as a lexical variable name in an async function.");
shouldThrowSyntaxError("AsyncFunction('const [await] = [];')", "Cannot use 'await' as a lexical variable name in an async function.");
shouldThrowSyntaxError("AsyncFunction('const [...await] = [];')", "Cannot use 'await' as a lexical variable name in an async function.");
shouldThrowSyntaxError("AsyncFunction('const {await} = {};')", "Cannot use 'await' as a lexical variable name in an async function.");
shouldThrowSyntaxError("AsyncFunction('const {isAsync: await} = {};')", "Cannot use 'await' as a lexical variable name in an async function.");
shouldThrowSyntaxError("AsyncFunction('const {isAsync: await} = {};')", "Cannot use 'await' as a lexical variable name in an async function.");
shouldThrowSyntaxError("AsyncFunction('function await() {}')", "Cannot declare function named 'await' in an async function.");
shouldThrowSyntaxError("AsyncFunction('async function await() {}')", "Cannot declare function named 'await' in an async function.");
shouldThrowSyntaxError("AsyncFunction('await', '')", "Cannot use 'await' as a parameter name in an async function.");
shouldThrowSyntaxError("AsyncFunction('[await]', '')", "Cannot use 'await' as a parameter name in an async function.");
shouldThrowSyntaxError("AsyncFunction('[...await]', '')", "Cannot use 'await' as a parameter name in an async function.");
shouldThrowSyntaxError("AsyncFunction('...await', '')", "Cannot use 'await' as a parameter name in an async function.");
shouldThrowSyntaxError("AsyncFunction('{await}', '')", "Cannot use 'await' as a parameter name in an async function.");
shouldThrowSyntaxError("AsyncFunction('{isAsync: await}', '')", "Cannot use 'await' as a parameter name in an async function.");
