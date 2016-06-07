function test(result, expected, message) {
    if (result !== expected)
        throw "Error: " + message + ". was: " + result + " wanted: " + expected;
}

function evalWithThrow(text) {
    var result; 
    try {
        result = eval(text);
    } catch (error) {
        return error.toString();
    }
    return result;
}

test(evalWithThrow('typeof function(,){ return a; }'), 'SyntaxError: Unexpected token \',\'. Expected a parameter pattern or a \')\' in parameter list.');
test(evalWithThrow('typeof function(a,,){ return a; }'), 'SyntaxError: Unexpected token \',\'. Expected a parameter pattern or a \')\' in parameter list.');
test(evalWithThrow('function a(a, ...last,){ return; }'), 'SyntaxError: Unexpected token \',\'. Rest parameter should be the last parameter in a function declaration.');
test(eval('typeof function(a,){ return a; }'), 'function');
test(eval('typeof function(a, b,){ return a + b; }'), 'function');
test(eval('typeof function(a, b, c, ){ return a + b + c; }'), 'function');

test(evalWithThrow('typeof ((,)=>{ return a; })'), 'SyntaxError: Unexpected token \',\'');
test(evalWithThrow('typeof ((a,,)=>{ return a; })'), 'SyntaxError: Unexpected token \',\'');
test(evalWithThrow('typeof ((a, ...last,)=>{ return a; })'), 'SyntaxError: Unexpected token \'...\'');
test(eval('typeof ((a,)=>{ return a; })'), 'function');
test(eval('typeof ((a, b,)=>{ return a + b; })'), 'function');
test(eval('typeof ((a, b, c)=>{ return a + b + c; })'), 'function');

test(evalWithThrow('typeof ((,)=>a)'), 'SyntaxError: Unexpected token \',\'');
test(evalWithThrow('typeof ((a,,)=>a)'), 'SyntaxError: Unexpected token \',\'');
test(evalWithThrow('(a,...last,)=>0;'), 'SyntaxError: Unexpected token \'...\'');
test(eval('typeof ((a,)=>a)'), 'function');
test(eval('typeof ((a, b,)=>a + b)'), 'function');
test(eval('typeof ((a, b, c)=>a + b + c)'), 'function');

test(evalWithThrow('typeof ((,)=>a)'), 'SyntaxError: Unexpected token \',\'');
test(evalWithThrow('typeof ((a,,)=>a)'), 'SyntaxError: Unexpected token \',\'');
test(evalWithThrow('(a,...last,)=>0;'), 'SyntaxError: Unexpected token \'...\'');
test(eval('typeof ((a,)=>a)'), 'function');
test(eval('typeof ((a, b,)=>a + b)'), 'function');
test(eval('typeof ((a, b, c)=>a + b + c)'), 'function');

test(evalWithThrow('typeof function(a = "x0",,){ return a; }'), 'SyntaxError: Unexpected token \',\'. Expected a parameter pattern or a \')\' in parameter list.');
test(evalWithThrow('typeof function(a = "x0",...last,){ return a; }'), 'SyntaxError: Unexpected token \',\'. Rest parameter should be the last parameter in a function declaration.');
test(eval('typeof function(a = "x0",){ return a; }'), 'function');
test(eval('typeof function(a = "x1", b = "y1",){ return a + b; }'), 'function');
test(eval('typeof function(a = "x2", b = "y2", c = "z3"){ return a + b + c; }'), 'function');

test(evalWithThrow('(function(a){ return a; })(,)'), 'SyntaxError: Unexpected token \',\'');
test(evalWithThrow('(function(a){ return a; })("A",,)'), 'SyntaxError: Unexpected token \',\'');
test(eval('(function(a){ return a; })("A",)'), 'A');
test(eval('(function(a, b,){ return a + b; })("A", "B",)'), 'AB');
test(eval('(function(a, b, c){ return a + b + c; })("A", "B", "C",)'), 'ABC');

test(eval('(function(a){ return arguments.length; })("A",)'), 1);
test(eval('(function(a, b,){ return arguments.length; })("A", "B",)'), 2);
test(eval('(function(a, b, c){ return arguments.length; })("A", "B", "C",)'), 3);
test(eval('(function(a,) { }).length'), 1);
test(eval('(function(a, b, ) { }).length'), 2);
test(eval('(function(a, b, c, ) { }).length'), 3);


