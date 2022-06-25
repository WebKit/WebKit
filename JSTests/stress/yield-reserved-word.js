function shouldNotThrow(func) {
    let error;
    try {
        func();
    } catch (e) {
        error = e;
    }
    if (error)
        throw new Error(`bad error: ${String(error)}`);
}

function shouldThrow(func, errorMessage) {
    let errorThrown = false;
    let error;
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

function checkClassicNoSyntaxError(source) {
    shouldNotThrow(() => eval(source));
}

function checkClassicSyntaxError(source, errorMessage) {
    shouldThrow(() => eval(source), errorMessage);
}

function checkStrictSyntaxError(source, errorMessage) {
    shouldThrow(() => checkModuleSyntax(source), errorMessage);
}


function checkNoSyntaxErrorCases(source) {
    checkClassicNoSyntaxError(source);

    // A nested function within a generator is allowed to use the "yield" name again
    // within its body because they have FunctionBody[~Yield]. Same with method bodies.
    checkClassicNoSyntaxError(`function *gen() { function f() { ${source} } }`);
    checkClassicNoSyntaxError(`function *gen() { async function f() { ${source} } }`);
    checkClassicNoSyntaxError(`function *gen() { let f = () => { ${source} } }`);
    checkClassicNoSyntaxError(`function *gen() { let f = async () => { ${source} } }`);
    checkClassicNoSyntaxError(`function *gen() { var o = { f() { ${source} } } }`);
    checkClassicNoSyntaxError(`function *gen() { var o = { async f() { ${source} } } }`);
    checkClassicNoSyntaxError(`function *gen() { var o = { get f() { ${source} } } }`);
    checkClassicNoSyntaxError(`function *gen() { var o = { set f(x) { ${source} } } }`);
}


checkNoSyntaxErrorCases(`var yield`);
checkNoSyntaxErrorCases(`let yield`);
checkNoSyntaxErrorCases(`const yield = 1`);
checkNoSyntaxErrorCases(`var {yield} = {}`);
checkNoSyntaxErrorCases(`yield: 1`);
checkNoSyntaxErrorCases(`function yield(){}`);
checkNoSyntaxErrorCases(`function foo(yield){}`);

checkNoSyntaxErrorCases(`(class { *yield(){} })`); // GeneratorMethod allows "yield" due to PropertyName[?Yield].
checkNoSyntaxErrorCases(`function *yield(){}`); // GeneratorDeclaration allows "yield" name due to BindingIdentifier[?Yield].

checkNoSyntaxErrorCases(`var o = { yield(yield){ var yield } }`); // PropertyName[?Yield] ( UniqueFormalParameters[~Yield] ) { FunctionBody[~Yield] }
checkNoSyntaxErrorCases(`var o = { *yield(){} }`); // GeneratorMethod[?Yield]
checkNoSyntaxErrorCases(`var o = { async yield(){} }`); // AsyncMethod[?Yield]
checkNoSyntaxErrorCases(`var o = { get x(){ var yield } }`); // get PropertyName[?Yield] () { FunctionBody[~Yield] }
checkNoSyntaxErrorCases(`var o = { set x(yield){} }`); // set PropertyName[?Yield] ( PropertySetParameterList) { FunctionBody[~Yield] }
checkNoSyntaxErrorCases(`var o = { set x(yield){} }`); // PropertySetParameterList : FormalParameter[~Yield]


// Disallowed inside a generator.

checkClassicSyntaxError(`
function* foo() { yield: 1; }
`, `SyntaxError: Cannot use 'yield' as a label in a generator function.`);

checkClassicSyntaxError(`
function* foo() { var yield; }
`, `SyntaxError: Cannot use 'yield' as a variable name in a generator function.`);

checkClassicSyntaxError(`
function* foo() { let yield; }
`, `SyntaxError: Cannot use 'yield' as a lexical variable name in a generator function.`);

checkClassicSyntaxError(`
function* foo() { var {yield} = {}; }
`, `SyntaxError: Cannot use abbreviated destructuring syntax for keyword 'yield'.`);

checkClassicSyntaxError(`
function* foo(yield){}
`, `SyntaxError: Cannot use 'yield' as a parameter name in a generator function.`);

// GeneratorExpression BindingIdentifier[+Yield] on the name.
checkClassicSyntaxError(`
(function* yield() { })
`, `SyntaxError: Cannot declare generator function named 'yield'.`);

// GeneratorDeclaration BindingIdentifier[?Yield] on the name.
checkClassicSyntaxError(`
function* foo() { function* yield(){} }
`, `SyntaxError: Cannot use 'yield' as a generator function name in a generator function.`);

// class BindingIdentifier[?Yield] on the name.
checkClassicSyntaxError(`
function* gen() { (class yield {}) }
`, `SyntaxError: Unexpected keyword 'yield'. Expected opening '{' at the start of a class body.`);


// Disallowed in strict code.

checkStrictSyntaxError(`
function* foo() { yield: 1; }
`, `SyntaxError: Cannot use 'yield' as a label in strict mode.:2`);

checkStrictSyntaxError(`
var yield;
`, `SyntaxError: Cannot use 'yield' as a variable name in strict mode.:2`);

checkStrictSyntaxError(`
let yield;
`, `SyntaxError: Cannot use 'yield' as a lexical variable name in strict mode.:2`);

checkStrictSyntaxError(`
var {yield} = {};
`, `SyntaxError: Cannot use abbreviated destructuring syntax for keyword 'yield'.:2`);

checkStrictSyntaxError(`
yield: 1
`, `SyntaxError: Cannot use 'yield' as a label in strict mode.:2`);

checkStrictSyntaxError(`
import {yield} from 'foo'
`, `SyntaxError: Cannot use keyword as imported binding name.:2`);

checkStrictSyntaxError(`
function* foo(yield){}
`, `SyntaxError: Cannot use 'yield' as a parameter name in strict mode.:2`);

checkStrictSyntaxError(`
function* yield(){}
`, `SyntaxError: Cannot use 'yield' as a generator function name in strict mode.:2`);

checkStrictSyntaxError(`
(function* yield(){})
`, `SyntaxError: Cannot use 'yield' as a generator function name in strict mode.:2`);

checkStrictSyntaxError(`
function* gen() { (class yield {}) }
`, `SyntaxError: Unexpected keyword 'yield'. Expected opening '{' at the start of a class body.:2`);

checkClassicSyntaxError(`
function *get() { var o = { yield }; }
`, `SyntaxError: Cannot use 'yield' as a shorthand property name in a generator function.`);


// Edge cases where ~Yield re-enables use of yield in non-strict code.

// FunctionDeclaration[Yield]:
//   function BindingIdentifier[?Yield] ( FormalParameters[~Yield] ) { FunctionBody[~Yield] }
checkClassicSyntaxError(`function *gen() { function yield() {} }`, `SyntaxError: Unexpected keyword 'yield'`);
checkClassicNoSyntaxError(`function *gen() { function f(yield) {} }`);

// FunctionExpression:
//   function BindingIdentifier[~Yield]opt ( FormalParameters[~Yield] ) { FunctionBody[~Yield] }
checkClassicNoSyntaxError(`function *gen() { (function yield() {}) }`);
checkClassicNoSyntaxError(`function *gen() { (function f(yield) {}) }`)
checkClassicNoSyntaxError(`function *gen() { (function yield(yield) {}) }`)
checkClassicNoSyntaxError(`function *gen() { (function(yield) {}) }`);

// AsyncFunctionDeclaration[Yield]:
//     async function BindingIdentifier[?Yield] ( FormalParameters[~Yield]) { AsyncFunctionBody }
checkClassicSyntaxError(`function *gen() { async function yield() {} }`, `SyntaxError: Unexpected keyword 'yield'`);
checkClassicNoSyntaxError(`function *gen() { async function f(yield) {} }`);

// AsyncFunctionExpression:
//     async function BindingIdentifier[~Yield]opt ( FormalParameters[~Yield] ) { AsyncFunctionBody }
checkClassicNoSyntaxError(`function *gen() { (async function yield() {}) }`);
checkClassicNoSyntaxError(`function *gen() { (async function f(yield) {}) }`)
checkClassicNoSyntaxError(`function *gen() { (async function yield(yield) {}) }`);
checkClassicNoSyntaxError(`function *gen() { (async function(yield) {}) }`);

// ArrowFunction[Yield]:
//     ArrowParameters[?Yield] => ConciseBody
checkClassicSyntaxError(`function *gen() { let f = (yield) => {} }`, `SyntaxError: Cannot use 'yield' as a parameter name in a generator function.`);

// ArrowFunction[Yield]:
//     ArrowParameters[?Yield] => ConciseBody
checkClassicSyntaxError(`function *gen() { let f = (yield) => {} }`, `SyntaxError: Cannot use 'yield' as a parameter name in a generator function.`);

// AsyncArrowFunction[Yield]:
//     async AsyncArrowBindingIdentifier[?Yield] => AsyncConciseBody
checkClassicSyntaxError(`function *gen() { let f = async (yield) => {} }`, `SyntaxError: Cannot use 'yield' as a parameter name in a generator function.`);
