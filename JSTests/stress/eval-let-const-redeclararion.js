// author: Oleksandr Skachkov <gskachkov@gmail.com>

var noError = false;
function assert(cond, msg) {
    if (!cond)
        throw new Error(msg || "broke assertion");
}
try {
    (function () { let a; eval('var a'); })();
} catch (e) {
    noError = e instanceof SyntaxError;
}
assert(noError, 'Expected syntax error in case var tried shadow let/const');
noError = false;
try {
    (function () { let a; eval('{ var a = 10; }'); })();
} catch (e) {
    noError = e instanceof SyntaxError;
}
assert(noError, 'Expected syntax error in case var tried shadow let/const');
noError = false;
try {
    function foo() { let a; eval('{ var a = 10; }'); }
    foo();
} catch (e) {
    noError = e instanceof SyntaxError;
}
assert(noError, 'Expected syntax error in case var tried shadow let/const');
noError = false;
try {
    (function () { const a = ''; eval('var a'); })();
} catch (e) {
    noError = e instanceof SyntaxError;
}
assert(noError, 'Expected syntax error in case var tried shadow let/const');
noError = false;
try {
    (function () { const a = ''; eval('{ var a = 10; }'); })();
} catch (e) {
    noError = e instanceof SyntaxError;
}
assert(noError, 'Expected syntax error in case var tried shadow let/const');
noError = false;
try {
    function boo() { const a = ''; eval('{ var a = 10; }'); }
    boo();
} catch (e) {
    noError = e instanceof SyntaxError;
}
assert(noError, 'Expected syntax error in case var tried shadow let/const');
noError = false;
function goo() {
    try {
        throw new Error('error');
    } catch (e) {
        noError = e instanceof Error;
        eval('var e = 10;');
        noError = noError && e === 10;
    }
    noError = noError && typeof e === 'undefined';
}
goo();
assert(noError, 'Expected that var in eval can override variable in catch block');
noError = false;
function baz() {
    try {
        throw new Error('error');
    } catch (e) {
        noError = e instanceof Error;
        eval('function e() { return "abcd"; }');
        noError = noError && e instanceof Error;
    }
    noError = noError && e() === 'abcd';
}
baz();
assert(noError, 'Expected that function in eval can\'t override variable in catch block');
noError = false;
function caz() {
    try {
        throw new Error('error');
    } catch (e) {
        noError = e instanceof Error;
        eval('function e2() { return "abcde"; } \n  eval(" function e() { return \'abcd\'; }");');
        noError = noError && e instanceof Error;
    }
    noError = noError && e() === 'abcd';
}
caz();
assert(noError, 'Expected that function in eval can\'t override variable in catch block');
noError = false;
function eaz() {
    try {
        throw new Error('error');
    } catch (e) {
        noError = e instanceof Error;
        eval('function e() { return "abcde"; } \n  eval(" function e() { return \'abcd\'; }");');
        noError = noError && e instanceof Error;
    }
    noError = noError && e() === 'abcd';
}
eaz();
assert(noError, 'Expected that function in eval can\'t override variable in catch block');
noError = false;
function daz() {
    try {
        throw new Error('error');
    } catch (e) {
        noError = e instanceof Error;
        {
            let obj = { e: 1234 };
            noError = noError && obj.e === 1234;
            with (obj) {
                eval('function e() { return "abcde"; }');
            }
            noError = noError && obj.e === 1234 && e instanceof Error;
        } 
        noError = noError && e instanceof Error;
    }
    noError = noError && e() === 'abcde';
}
daz();
assert(noError, 'Expected that function in eval can\'t override variable in catch block, in block scope and with scope');
noError = false;
function faz() {
    try {
        throw new Error('error');
    } catch (e) {
        noError = e instanceof Error;
        {
            let e = 1234;
            noError = noError && e === 1234;
            try {
                eval('function e() { return "abcde"; }');
                noError = false;
            } catch (error) {
                noError = noError && error instanceof SyntaxError;
            }
        }
        noError = noError && e instanceof Error;
    }
    noError = noError && typeof e === 'undefined';
}
faz();
assert(noError, 'Expected that function in eval can\'t override variable in catch block and raise exception if faced wihh let variable');
noError = false;
function gaz() {
    var e = 4321;
    try {
        throw new Error('error');
    } catch (e) {
        noError = e instanceof Error;
        eval('function e() { return "abcde"; }');
        noError = noError && e instanceof Error;
    }
    noError = noError && e() === 'abcde';
}
gaz();
assert(noError, 'Expected that function in eval can override variable in catch block');
noError = false;
function jaz() {
    try {
        throw new Error('error');
    } catch (e) {
        noError = e instanceof Error;
        eval('{ function e() { return "abcd"; } }');
        noError = noError && e instanceof Error;
    }
    noError = noError && e() === 'abcd';
}
jaz();
assert(noError, 'Expected that function in eval can\'t override variable in catch block');
noError = false;
try {
    (function () {
        var o = { a: '1' };
        let a;
        with (o) {
            eval('var a');
        }
    })();
} catch (error) {
    noError = error instanceof SyntaxError;
}
assert(noError, 'Expected `with` is not affect early SyntaxError in eval');
noError = false;
(function () {
    var o = { a: '1' };
    with (o) {
        eval('var a = 10;');
    }
    noError = a === undefined && o.a === 10;
})();
assert(noError, 'Expected `with` is not affect early SyntaxError in eval');
noError = false;
{
    let a = 10;
    noError = a === 10;
    (function () {
        noError = noError && a === 10;
        eval('var a = 20');
        noError = noError && a === 20;
    })();
    noError = noError && a === 10;
}
assert(noError, 'Expected `with` is not affect early SyntaxError in eval');
noError = false;
{
    const a = 10;
    noError = a === 10;
    (function () {
        noError = noError && a === 10;
        eval('var a = 20');
        noError = noError && a === 20;
    })();
    noError = noError && a === 10;
}
assert(noError, 'Expected `with` is not affect early SyntaxError in eval');
