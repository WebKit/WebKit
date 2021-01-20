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
    checkSyntax('var a1 = async function*asyncGenWithName1(){ for await(const value of foo()) {} }');
    checkSyntax('var a1 = async function asyncWithName1(){ for await(const value of foo()) {} }');
    checkSyntax('var a1 = async function*asyncGenWithName1(){ for await(let value of foo()) {} }');
    checkSyntax('var a1 = async function asyncWithName1(){ for await(let value of foo()) {} }');
})();

(function checkSimpleAsyncGeneratorStrictMode() {
    checkSyntax('"use strict"; var a1 = async function*asyncGenWithName1(){ for await(const value of foo()) {}  }');
    checkSyntax('"use strict"; var a1 = async function asyncWithName1(){ for await(const value of foo()) {} }');
    checkSyntax('"use strict"; var a1 = async function*asyncGenWithName1(){ for await(let value of foo()) {} }');
    checkSyntax('"use strict"; var a1 = async function asyncWithName1(){ for await(let value of foo()) {} }');
})();


(function checkNestedAsyncGenerators() { 
    var wrappers = [
        {start: 'var a1 = async function*asyncGenWithName1(){', finish: '}'},
        {start: 'async function*asyncGenWithName2(){ ', finish: '}'},
        {start: 'async function asyncWithName2(){ ', finish: '}'},
        {start: 'class A { async * method() { ', finish: ' } }'},
        {start: 'var a1 = async () => {', finish: '}'},
        {start: 'var a1 = async () => { try {   ', finish: ' } catch (e) {} }'},
        {start: 'var a1 = async () => { {   ', finish: ' } }'},
        {start: 'var a1 = async () => { if (true) {   ', finish: ' } }'},
        {start: 'var a1 = async () => { if (true) ', finish: ' }'},
        {start: 'var a1 = async () => { if (true) foo(); else { ', finish: ' } }'},
        {start: 'var a1 = async () => { while (true) { ', finish: ' } }'},
        {start: 'var a1 = async () => { for(;;) { ', finish: ' } }'},
        {start: 'var a1 = async () => { switch(e) { case \'1\' :  ', finish: ' } }'},
    ];

    expressions = [
        'for await(const value of foo()) {}',
        'for await(let value of foo()) {}',
        'for await(var value of foo()) {}',
        'for await(var [a, b] of foo()) {}',
        'for await(let {a, b} of foo()) {}',
        'for await(const [... b] of foo()) {}',
        'for await(const [,,, b] of foo()) {}',
        'for await(const value of boo) {}',
        'for await(let value of boo) {}',
        'for await(const value of foo().boo()) {}',
        'for await(let value of foo.boo()) {}',
        'for await(let value of foo.boo(value)) {}',
        'for await(let value of [1,2,3]) {}',
        'for await(value of [1,2,3]) {}',
        'for await(value of x + x) {}',
        'for await(value of f()) {}',
        'for await(value of (x + x)) {}',
    ];

    wrappers.forEach(wrapper => {
        expressions.forEach(exp => {
            checkSyntax(wrapper.start + exp + wrapper.finish);
        });
    })
})();


(function checkSimpleAsyncGeneratorSyntaxErrorInSloppyMode() {
    checkSyntaxError("var asyncGenFn = function () { for await(const value of foo()) {} }");
    checkSyntaxError("var asyncGenFn = async function () { var arr = () => { for await(const value of foo()) {} } }");
    checkSyntaxError("var asyncGenFn = function* () { for await(const value of foo()) {} }");
    checkSyntaxError("var asyncGenFn = async function* () { var arr = () => { for await(const value of foo()) {} } }");
    checkSyntaxError('var a1 = async function*asyncGenWithName1(){ for await(const value in foo()) {} }');
    checkSyntaxError('var a1 = async function asyncWithName1(){ for await(const value in foo()) {} }');
    checkSyntaxError('var a1 = async function asyncWithName1(){ for await (;;) {} }');
    checkSyntaxError("var a1 = async function asyncWithName1(){ for await (let v = 4;;) {} }");
    checkSyntaxError("var a1 = async function asyncWithName1(){ for await (let v of f();;) {} }");
    checkSyntaxError("var a1 = async function asyncWithName1(){ for await (let v of boo;;) {} }");
    checkSyntaxError("var a1 = async function asyncWithName1(){ for await (let v of boo of) {} }");
    checkSyntaxError("async function asyncWithName1(){ for await (let v of boo in) {} }");
    checkSyntaxError("async function asyncWithName1(){ for await (v in x + x ) {} }");
})();

(function checkSimpleAsyncGeneratorSyntaxErrorInStrictMode() {
    checkSyntaxError("'use strict'; var asyncGenFn = function () { for await(const value of foo()) {} }");
    checkSyntaxError("'use strict'; var asyncGenFn = async function () { var arr = () => { for await(const value of foo()) {} } }");
    checkSyntaxError("'use strict'; var asyncGenFn = function* () { for await(const value of foo()) {} }");
    checkSyntaxError("'use strict'; var asyncGenFn = async function* () { var arr = () => { for await(const value of foo()) {} } }");
    checkSyntaxError("'use strict'; var a1 = async function*asyncGenWithName1(){ for await(const value in foo()) {} }");
    checkSyntaxError("'use strict'; var a1 = async function asyncWithName1(){ for await(const value in foo()) {} }");
    checkSyntaxError("'use strict'; var a1 = async function asyncWithName1(){ for await (;;) {} }");
    checkSyntaxError("'use strict'; var a1 = async function asyncWithName1(){ for await (let v = 4;;) {} }");
    checkSyntaxError("'use strict'; var a1 = async function asyncWithName1(){ for await (let v of f();;) {} }");
    checkSyntaxError("'use strict'; var a1 = async function asyncWithName1(){ for await (let v of boo;;) {} }");
    checkSyntaxError("'use strict'; var a1 = async function asyncWithName1(){ for await (let v of boo of) {} }");
    checkSyntaxError("'use strict'; async function asyncWithName1(){ for await (let v of boo in) {} }");
    checkSyntaxError("'use strict'; async function asyncWithName1(){ for await (v in x + x ) {} }");
})();
