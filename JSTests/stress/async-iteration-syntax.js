// This test requires enableAsyncIterator to be enabled at run time.
//@ skip


var assert = function (result, expected, message = "") {
  if (result !== expected) {
    throw new Error('Error in assert. Expected "' + expected + '" but was "' + result + '":' + message );
  }
};


function checkSyntax(src) {
    try {
        eval(src);
    } catch (error) {
        if (error instanceof SyntaxError)
            throw new Error("Syntax Error: " + String(error) + "\n script: `" + src + "`");
    }
}

function checkSyntaxError(src, message) {
    var bError = false;
    try {
        eval(src);
    } catch (error) {
        bError = error instanceof SyntaxError && (String(error) === message || typeof message === 'undefined'); 
    }
    if (!bError) {
        throw new Error("Expected syntax Error: " + message + "\n in script: `" + src + "`");
    }
}

(function checkSimpleAsyncGeneratorSloppyMode() {
    checkSyntax('var a1 = async function*asyncGenWithName1(){}');
    checkSyntax('var a2 = async function *asyncGenWithName2(){ yield 11; }');
    checkSyntax('var a3 = async function * asyncGenWithName2(){ await p; yield 11; }');
    checkSyntax('var d1 = async function*(){}');
    checkSyntax('var d2 = async function* (){ yield 11; }');
    checkSyntax('var d3 = async function * (){ await p; yield 11; }');
    checkSyntax('async function* withName1(){  }');
    checkSyntax('async function *withName2(){ yield 11; }');
    checkSyntax('async function * withName3(){ await p; yield 11; }');
    checkSyntax('class A { async * method() { } }');
    checkSyntax('class B { async * method() {yield 11;} }');
    checkSyntax('class C { async * method() {yield 11; await p;} }');
    checkSyntax('class D { async * "method"() {yield 11; await p;} }');
    checkSyntax('class F { async * 0() {yield 11; await p;} }');
    checkSyntax('var obj = { async * method() {yield 11; await p;} }');
    checkSyntax('({ async foo() {} })');
    checkSyntax('({ async : 1 })');
})();

(function checkSimpleAsyncGeneratorStrictMode() {
    checkSyntax('"use strict"; var a1 = async function*asyncGenWithName1(){}');
    checkSyntax('"use strict"; var a2 = async function *asyncGenWithName2(){ yield 11; }');
    checkSyntax('"use strict"; var a3 = async function * asyncGenWithName2(){ await p; yield 11; }');
    checkSyntax('"use strict"; var d1 = async function*(){}');
    checkSyntax('"use strict"; var d2 = async function* (){ yield 11; }');
    checkSyntax('"use strict"; var d3 = async function * (){ await p; yield 11; }');
    checkSyntax('"use strict"; async function* withName1(){  }');
    checkSyntax('"use strict"; async function *withName2(){ yield 11; }');
    checkSyntax('"use strict"; async function * withName3(){ await p; yield 11; }');
    checkSyntax('"use strict"; class A { async * method() { } }');
    checkSyntax('"use strict"; class B { async * method() {yield 11;} }');
    checkSyntax('"use strict"; class C { async * method() {yield 11; await p;} }');
    checkSyntax('"use strict"; class D { async * "method"() {yield 11; await p;} }');
    checkSyntax('"use strict"; class E { async * ["calc" + "ulate"]() {yield 11; await p;} }');
    checkSyntax('"use strict"; class F { async * 0() {yield 11; await p;} }');
    checkSyntax('"use strict"; var obj = { async * method() {yield 11; await p;} }');
    checkSyntax('"use strict"; ({ async foo() {} })');
    checkSyntax('"use strict"; ({ async : 1 })');
})();


(function checkNestedAsyncGenerators() { 
    var wrappers = [
        {start: 'var a1 = async function*asyncGenWithName1(){', finish: '}'},
        {start: 'async function*asyncGenWithName2(){ ', finish: '}'},
        {start: 'class A { async * method() { ', finish: ' } }'}
    ];

    expressions = [
        'await 10; yield 11; return 12;',
        'var async = 10; yield async;',
        'var async = 10; await async;',
        'var async = 10; return async;',
        'var async = function() {}; return async;',
        'var async = function() {}; return async();',
    ];

    wrappers.forEach(wrapper => {
        expressions.forEach(exp => {
            checkSyntax(wrapper.start + exp + wrapper.finish);
        });
    })
})();


(function checkSimpleAsyncGeneratorSyntaxErrorInSloppyMode() {
    checkSyntaxError("var asyncGenFn = async function *await() {}");
    checkSyntaxError("var asyncGenFn = async function*(await) {}");
    checkSyntaxError("var asyncGenFn = async function *withName(await) {}");
    checkSyntaxError("async function *asyncGeneratorFunctionDeclaration(await) {}");
    checkSyntaxError("var asyncGenFn = *async function () {}");
    checkSyntaxError("var asyncGenFn = *async function withName() {}");
    checkSyntaxError("*async function asyncGeneratorFunctionDeclaration(await) {}");
    checkSyntaxError("var obj = { *async asyncGeneratorMethod() {} };");
    checkSyntaxError("var obj = { async asyncGeneratorMethod*() {} };");
    checkSyntaxError("class A { get async* ttt() {} }");
    checkSyntaxError("class B { get *async ttt() {} }");
    checkSyntaxError('({ async = 1 })');
})();

(function checkSimpleAsyncGeneratorSyntaxErrorInStrictMode() {
    checkSyntaxError("'use strict'; var asyncGenFn = async function *await() {}");
    checkSyntaxError("'use strict'; var asyncGenFn = async function*(await) {}");
    checkSyntaxError("'use strict'; var asyncGenFn = async function *withName(await) {}");
    checkSyntaxError("'use strict'; async function *asyncGeneratorFunctionDeclaration(await) {}");
    checkSyntaxError("'use strict'; var asyncGenFn = *async function () {}");
    checkSyntaxError("'use strict'; var asyncGenFn = *async function withName() {}");
    checkSyntaxError("'use strict'; *async function asyncGeneratorFunctionDeclaration(await) {}");
    checkSyntaxError("'use strict'; var obj = { *async asyncGeneratorMethod() {} };");
    checkSyntaxError("'use strict'; var obj = { async asyncGeneratorMethod*() {} };");
    checkSyntaxError("'use strict'; class A { get async* ttt() {} }");
    checkSyntaxError("'use strict'; class B { get *async ttt() {} }");
    checkSyntaxError("'use strict'; ({ async = 1 })");
})();
