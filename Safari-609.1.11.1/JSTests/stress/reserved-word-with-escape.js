function testSyntax(script) {
    try {
        eval(script);
    } catch (error) {
        if (error instanceof SyntaxError)
            throw new Error("Bad error: " + String(error));
    }
}

function testSyntaxError(script, message) {
    var error = null;
    try {
        eval(script);
    } catch (e) {
        error = e;
    }
    if (!error)
        throw new Error("Expected syntax error not thrown");

    if (String(error) !== message)
        throw new Error("Bad error: " + String(error));
}

testSyntax("var cocoa");
testSyntax("var c\u006fcoa");

testSyntaxError(String.raw`var var`, String.raw`SyntaxError: Cannot use the keyword 'var' as a variable name.`);
testSyntaxError(String.raw`var v\u0061r`, String.raw`SyntaxError: Unexpected escaped characters in keyword token: 'v\u0061r'`);
testSyntaxError(String.raw`var v\u{0061}r`, String.raw`SyntaxError: Unexpected escaped characters in keyword token: 'v\u{0061}r'`);

testSyntaxError(String.raw`var var = 2000000;`, String.raw`SyntaxError: Cannot use the keyword 'var' as a variable name.`);
testSyntaxError(String.raw`var v\u0061r = 2000000;`, String.raw`SyntaxError: Unexpected escaped characters in keyword token: 'v\u0061r'`);
testSyntaxError(String.raw`var v\u{0061}r = 2000000`, String.raw`SyntaxError: Unexpected escaped characters in keyword token: 'v\u{0061}r'`);

testSyntaxError(String.raw`var {var} = obj)`, String.raw`SyntaxError: Cannot use abbreviated destructuring syntax for keyword 'var'.`);
testSyntaxError(String.raw`var {v\u0061r} = obj`, String.raw`SyntaxError: Unexpected escaped characters in keyword token: 'v\u0061r'`);
testSyntaxError(String.raw`var {v\u{0061}r} = obj`, String.raw`SyntaxError: Unexpected escaped characters in keyword token: 'v\u{0061}r'`);

testSyntaxError(String.raw`var {var:var} = obj)`, String.raw`SyntaxError: Cannot use the keyword 'var' as a variable name.`);
testSyntaxError(String.raw`var {var:v\u0061r} = obj`, String.raw`SyntaxError: Unexpected escaped characters in keyword token: 'v\u0061r'`);
testSyntaxError(String.raw`var {var:v\u{0061}r} = obj`, String.raw`SyntaxError: Unexpected escaped characters in keyword token: 'v\u{0061}r'`);

testSyntaxError(String.raw`var [var] = obj`, String.raw`SyntaxError: Cannot use the keyword 'var' as a variable name.`);
testSyntaxError(String.raw`var [v\u0061r] = obj`, String.raw`SyntaxError: Unexpected escaped characters in keyword token: 'v\u0061r'`);
testSyntaxError(String.raw`var [v\u{0061}r] = obj`, String.raw`SyntaxError: Unexpected escaped characters in keyword token: 'v\u{0061}r'`);

testSyntaxError(String.raw`[var] = obj`, String.raw`SyntaxError: Unexpected keyword 'var'`);
testSyntaxError(String.raw`[v\u0061r] = obj`, String.raw`SyntaxError: Unexpected escaped characters in keyword token: 'v\u0061r'`);
testSyntaxError(String.raw`[v\u{0061}r] = obj`, String.raw`SyntaxError: Unexpected escaped characters in keyword token: 'v\u{0061}r'`);

testSyntaxError(String.raw`function var() { }`, String.raw`SyntaxError: Cannot use the keyword 'var' as a function name.`);
testSyntaxError(String.raw`function v\u0061r() { }`, String.raw`SyntaxError: Unexpected escaped characters in keyword token: 'v\u0061r'`);
testSyntaxError(String.raw`function v\u{0061}r() { }`, String.raw`SyntaxError: Unexpected escaped characters in keyword token: 'v\u{0061}r'`);

testSyntaxError(String.raw`function a(var) { }`, String.raw`SyntaxError: Cannot use the keyword 'var' as a parameter name.`);
testSyntaxError(String.raw`function a(v\u0061r) { }`, String.raw`SyntaxError: Unexpected escaped characters in keyword token: 'v\u0061r'`);
testSyntaxError(String.raw`function a(v\u{0061}r) { }`, String.raw`SyntaxError: Unexpected escaped characters in keyword token: 'v\u{0061}r'`);

testSyntaxError(String.raw`function a({var}) { }`, String.raw`SyntaxError: Cannot use abbreviated destructuring syntax for keyword 'var'.`);
testSyntaxError(String.raw`function a({v\u0061r}) { }`, String.raw`SyntaxError: Unexpected escaped characters in keyword token: 'v\u0061r'`);
testSyntaxError(String.raw`function a({v\u{0061}r}) { }`, String.raw`SyntaxError: Unexpected escaped characters in keyword token: 'v\u{0061}r'`);

testSyntaxError(String.raw`function a({var:var}) { }`, String.raw`SyntaxError: Cannot use the keyword 'var' as a parameter name.`);
testSyntaxError(String.raw`function a({var:v\u0061r}) { }`, String.raw`SyntaxError: Unexpected escaped characters in keyword token: 'v\u0061r'`);
testSyntaxError(String.raw`function a({var:v\u{0061}r}) { }`, String.raw`SyntaxError: Unexpected escaped characters in keyword token: 'v\u{0061}r'`);

testSyntaxError(String.raw`function a([var]) { }`, String.raw`SyntaxError: Cannot use the keyword 'var' as a parameter name.`);
testSyntaxError(String.raw`function a([v\u0061r]) { }`, String.raw`SyntaxError: Unexpected escaped characters in keyword token: 'v\u0061r'`);
testSyntaxError(String.raw`function a([v\u{0061}r]) { }`, String.raw`SyntaxError: Unexpected escaped characters in keyword token: 'v\u{0061}r'`);

testSyntaxError(String.raw`(function var() { })`, String.raw`SyntaxError: Cannot use the keyword 'var' as a function name.`);
testSyntaxError(String.raw`(function v\u0061r() { })`, String.raw`SyntaxError: Unexpected escaped characters in keyword token: 'v\u0061r'`);
testSyntaxError(String.raw`(function v\u{0061}r() { })`, String.raw`SyntaxError: Unexpected escaped characters in keyword token: 'v\u{0061}r'`);

testSyntaxError(String.raw`(function a(var) { })`, String.raw`SyntaxError: Cannot use the keyword 'var' as a parameter name.`);
testSyntaxError(String.raw`(function a(v\u0061r) { })`, String.raw`SyntaxError: Unexpected escaped characters in keyword token: 'v\u0061r'`);
testSyntaxError(String.raw`(function a(v\u{0061}r) { })`, String.raw`SyntaxError: Unexpected escaped characters in keyword token: 'v\u{0061}r'`);

testSyntaxError(String.raw`(function a({var}) { })`, String.raw`SyntaxError: Cannot use abbreviated destructuring syntax for keyword 'var'.`);
testSyntaxError(String.raw`(function a({v\u0061r}) { })`, String.raw`SyntaxError: Unexpected escaped characters in keyword token: 'v\u0061r'`);
testSyntaxError(String.raw`(function a({v\u{0061}r}) { })`, String.raw`SyntaxError: Unexpected escaped characters in keyword token: 'v\u{0061}r'`);

testSyntaxError(String.raw`(function a({var:var}) { })`, String.raw`SyntaxError: Cannot use the keyword 'var' as a parameter name.`);
testSyntaxError(String.raw`(function a({var:v\u0061r}) { })`, String.raw`SyntaxError: Unexpected escaped characters in keyword token: 'v\u0061r'`);
testSyntaxError(String.raw`(function a({var:v\u{0061}r}) { })`, String.raw`SyntaxError: Unexpected escaped characters in keyword token: 'v\u{0061}r'`);

testSyntaxError(String.raw`(function a([var]) { })`, String.raw`SyntaxError: Cannot use the keyword 'var' as a parameter name.`);
testSyntaxError(String.raw`(function a([v\u0061r]) { })`, String.raw`SyntaxError: Unexpected escaped characters in keyword token: 'v\u0061r'`);
testSyntaxError(String.raw`(function a([v\u{0061}r]) { })`, String.raw`SyntaxError: Unexpected escaped characters in keyword token: 'v\u{0061}r'`);

testSyntaxError(String.raw`(function a([{var}]) { })`, String.raw`SyntaxError: Cannot use abbreviated destructuring syntax for keyword 'var'.`);
testSyntaxError(String.raw`(function a([{v\u0061r}]) { })`, String.raw`SyntaxError: Unexpected escaped characters in keyword token: 'v\u0061r'`);
testSyntaxError(String.raw`(function a([{v\u{0061}r}]) { })`, String.raw`SyntaxError: Unexpected escaped characters in keyword token: 'v\u{0061}r'`);

testSyntaxError(String.raw`(function a([{var:var}]) { })`, String.raw`SyntaxError: Cannot use the keyword 'var' as a parameter name.`);
testSyntaxError(String.raw`(function a([{var:v\u0061r}]) { })`, String.raw`SyntaxError: Unexpected escaped characters in keyword token: 'v\u0061r'`);
testSyntaxError(String.raw`(function a([{var:v\u{0061}r}]) { })`, String.raw`SyntaxError: Unexpected escaped characters in keyword token: 'v\u{0061}r'`);

testSyntaxError(String.raw`(function a([[var]]) { })`, String.raw`SyntaxError: Cannot use the keyword 'var' as a parameter name.`);
testSyntaxError(String.raw`(function a([[v\u0061r]]) { })`, String.raw`SyntaxError: Unexpected escaped characters in keyword token: 'v\u0061r'`);
testSyntaxError(String.raw`(function a([[v\u{0061}r]]) { })`, String.raw`SyntaxError: Unexpected escaped characters in keyword token: 'v\u{0061}r'`);

testSyntaxError(String.raw`(function a({ hello: {var}}) { })`, String.raw`SyntaxError: Cannot use abbreviated destructuring syntax for keyword 'var'.`);
testSyntaxError(String.raw`(function a({ hello: {v\u0061r}}) { })`, String.raw`SyntaxError: Unexpected escaped characters in keyword token: 'v\u0061r'`);
testSyntaxError(String.raw`(function a({ hello: {v\u{0061}r}}) { })`, String.raw`SyntaxError: Unexpected escaped characters in keyword token: 'v\u{0061}r'`);

testSyntaxError(String.raw`(function a({ hello: {var:var}}) { })`, String.raw`SyntaxError: Cannot use the keyword 'var' as a parameter name.`);
testSyntaxError(String.raw`(function a({ hello: {var:v\u0061r}}) { })`, String.raw`SyntaxError: Unexpected escaped characters in keyword token: 'v\u0061r'`);
testSyntaxError(String.raw`(function a({ hello: {var:v\u{0061}r}}) { })`, String.raw`SyntaxError: Unexpected escaped characters in keyword token: 'v\u{0061}r'`);

testSyntaxError(String.raw`(function a({ hello: [var]}) { })`, String.raw`SyntaxError: Cannot use the keyword 'var' as a parameter name.`);
testSyntaxError(String.raw`(function a({ hello: [v\u0061r]}) { })`, String.raw`SyntaxError: Unexpected escaped characters in keyword token: 'v\u0061r'`);
testSyntaxError(String.raw`(function a({ hello: [v\u{0061}r]}) { })`, String.raw`SyntaxError: Unexpected escaped characters in keyword token: 'v\u{0061}r'`);

testSyntaxError(String.raw`(function a({ 0: {var} }) { })`, String.raw`SyntaxError: Cannot use abbreviated destructuring syntax for keyword 'var'.`);
testSyntaxError(String.raw`(function a({ 0: {v\u0061r}}) { })`, String.raw`SyntaxError: Unexpected escaped characters in keyword token: 'v\u0061r'`);
testSyntaxError(String.raw`(function a({ 0: {v\u{0061}r}}) { })`, String.raw`SyntaxError: Unexpected escaped characters in keyword token: 'v\u{0061}r'`);

testSyntaxError(String.raw`(function a({ 0: {var:var}}) { })`, String.raw`SyntaxError: Cannot use the keyword 'var' as a parameter name.`);
testSyntaxError(String.raw`(function a({ 0: {var:v\u0061r}}) { })`, String.raw`SyntaxError: Unexpected escaped characters in keyword token: 'v\u0061r'`);
testSyntaxError(String.raw`(function a({ 0: {var:v\u{0061}r}}) { })`, String.raw`SyntaxError: Unexpected escaped characters in keyword token: 'v\u{0061}r'`);

testSyntaxError(String.raw`(function a({ 0: {value:var}}) { })`, String.raw`SyntaxError: Cannot use the keyword 'var' as a parameter name.`);
testSyntaxError(String.raw`(function a({ 0: {value:v\u0061r}}) { })`, String.raw`SyntaxError: Unexpected escaped characters in keyword token: 'v\u0061r'`);
testSyntaxError(String.raw`(function a({ 0: {value:v\u{0061}r}}) { })`, String.raw`SyntaxError: Unexpected escaped characters in keyword token: 'v\u{0061}r'`);

testSyntaxError(String.raw`(function a({ 0: [var]}) { })`, String.raw`SyntaxError: Cannot use the keyword 'var' as a parameter name.`);
testSyntaxError(String.raw`(function a({ 0: [v\u0061r]}) { })`, String.raw`SyntaxError: Unexpected escaped characters in keyword token: 'v\u0061r'`);
testSyntaxError(String.raw`(function a({ 0: [v\u{0061}r]}) { })`, String.raw`SyntaxError: Unexpected escaped characters in keyword token: 'v\u{0061}r'`);

testSyntaxError(String.raw`try { } catch(var) { }`, String.raw`SyntaxError: Cannot use the keyword 'var' as a catch parameter name.`);
testSyntaxError(String.raw`try { } catch(v\u0061r) { }`, String.raw`SyntaxError: Unexpected escaped characters in keyword token: 'v\u0061r'`);
testSyntaxError(String.raw`try { } catch(v\u{0061}r) { }`, String.raw`SyntaxError: Unexpected escaped characters in keyword token: 'v\u{0061}r'`);

testSyntaxError(String.raw`class var { }`, String.raw`SyntaxError: Cannot use the keyword 'var' as a class name.`);
testSyntaxError(String.raw`class v\u0061r { }`, String.raw`SyntaxError: Unexpected escaped characters in keyword token: 'v\u0061r'`);
testSyntaxError(String.raw`class v\u{0061}r { }`, String.raw`SyntaxError: Unexpected escaped characters in keyword token: 'v\u{0061}r'`);

testSyntaxError(String.raw`({ v\u0061r: 'Cocoa' })`, String.raw`SyntaxError: Unexpected escaped characters in keyword token: 'v\u0061r'`);
testSyntaxError(String.raw`({ v\u{0061}r: 'Cocoa' })`, String.raw`SyntaxError: Unexpected escaped characters in keyword token: 'v\u{0061}r'`);

testSyntaxError(String.raw`{for(o i\u006E {}){}}`, String.raw`SyntaxError: Unexpected escaped characters in keyword token: 'i\u006E'`);
testSyntaxError(String.raw`{for(o i\u{006E} {}){}}`, String.raw`SyntaxError: Unexpected escaped characters in keyword token: 'i\u{006E}'`);


testSyntaxError(String.raw`implements`, String.raw`ReferenceError: Can't find variable: implements`);
testSyntaxError(String.raw`impleme\u006Ets`, String.raw`ReferenceError: Can't find variable: implements`);
testSyntaxError(String.raw`impleme\u{006E}ts`, String.raw`ReferenceError: Can't find variable: implements`);

testSyntaxError(String.raw`'use strict'; implements`, String.raw`SyntaxError: Unexpected use of reserved word 'implements' in strict mode`);
testSyntaxError(String.raw`'use strict'; impleme\u006Ets`, String.raw`SyntaxError: Unexpected escaped characters in keyword token: 'impleme\u006Ets'`);
testSyntaxError(String.raw`'use strict'; impleme\u{006E}ts`, String.raw`SyntaxError: Unexpected escaped characters in keyword token: 'impleme\u{006E}ts'`);
