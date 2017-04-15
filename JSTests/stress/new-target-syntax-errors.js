function assert(b) {
    if (!b)
        throw new Error("Bad assertion")
}

function shouldBeSyntaxError(str) {
    let failed = true;
    try {
        new Function(str);
    } catch(e) {
        if (e instanceof SyntaxError)
            failed = false;
    }
    
    if (failed)
        throw new Error("Did not throw syntax error: " + str);
}

function shouldNotBeSyntaxError(str) {
    let failed = false;
    try {
        new Function(str);
    } catch(e) {
        if (e instanceof SyntaxError && e.message.indexOf("new.target") !== -1)
            failed = true;
    }
    
    if (failed)
        throw new Error("Did throw a syntax error: " + str);
}


let operators = ["=", "+=", "-=", "*=", "<<=", ">>=", ">>>=", "&=", "^=", "|=", "%="];
for (let operator of operators) {
    let functionBody = `new.target ${operator} 20`;
    shouldBeSyntaxError(functionBody);

    functionBody = `foo = new.target ${operator} 20`;
    shouldBeSyntaxError(functionBody);

    functionBody = `foo ${operator} new.target ${operator} 20`;
    shouldBeSyntaxError(functionBody);

    functionBody = `new.target ${operator} foo *= 40`;
    shouldBeSyntaxError(functionBody);


    // Make sure our tests cases our sound and they should not be syntax errors if new.target is replaced by foo
    functionBody = `foo ${operator} 20`;
    shouldNotBeSyntaxError(functionBody);

    functionBody = `foo = foo ${operator} 20`;
    shouldNotBeSyntaxError(functionBody);

    functionBody = `foo ${operator} foo ${operator} 20`;
    shouldNotBeSyntaxError(functionBody);

    functionBody = `foo ${operator} foo *= 40`;
    shouldNotBeSyntaxError(functionBody);
}

let prePostFixOperators = ["++", "--"];
for (let operator of prePostFixOperators) {
    let functionBody = `${operator}new.target`;
    shouldBeSyntaxError(functionBody);

    functionBody = `foo = ${operator}new.target`;
    shouldBeSyntaxError(functionBody);

    functionBody = `${operator}foo`;
    shouldNotBeSyntaxError(functionBody);

    functionBody = `foo = ${operator}foo`;
    shouldNotBeSyntaxError(functionBody);
}

for (let operator of prePostFixOperators) {
    let functionBody = `new.target${operator}`;
    shouldBeSyntaxError(functionBody);

    functionBody = `foo = new.target${operator}`;
    shouldBeSyntaxError(functionBody);

    functionBody = `foo${operator}`;
    shouldNotBeSyntaxError(functionBody);

    functionBody = `foo = foo${operator}`;
    shouldNotBeSyntaxError(functionBody);
}

shouldBeSyntaxError(`({foo: new.target} = {foo:20})`);

// Scripts - 15.1.1 Static Semantics: Early Errors
// https://tc39.github.io/ecma262/#sec-scripts-static-semantics-early-errors
//
// Modules - 15.2.1.1 Static Semantics: Early Errors
// https://tc39.github.io/ecma262/#sec-module-semantics-static-semantics-early-errors
//
// new.target is not allowed in arrow functions in global scope.

let sawSyntaxError;

sawSyntaxError = false;
try {
    eval(`() => new.target`);
} catch(e) {
    sawSyntaxError = e instanceof SyntaxError;
}
assert(sawSyntaxError);

sawSyntaxError = false;
try {
    eval(`() => { new.target }`);
} catch(e) {
    sawSyntaxError = e instanceof SyntaxError;
}
assert(sawSyntaxError);

sawSyntaxError = false;
try {
    eval(`async () => new.target`);
} catch(e) {
    sawSyntaxError = e instanceof SyntaxError;
}
assert(sawSyntaxError);

sawSyntaxError = false;
try {
    eval(`async () => { new.target }`);
} catch(e) {
    sawSyntaxError = e instanceof SyntaxError;
}
assert(sawSyntaxError);

sawSyntaxError = false;
try {
    eval(`async () => await new.target`);
} catch(e) {
    sawSyntaxError = e instanceof SyntaxError;
}
assert(sawSyntaxError);

sawSyntaxError = false;
try {
    eval(`async () => { await new.target }`);
} catch(e) {
    sawSyntaxError = e instanceof SyntaxError;
}
assert(sawSyntaxError);

let sawError = false;
try {
    new Function(`() => new.target`);
    new Function(`() => { new.target }`);
    new Function(`async () => new.target`);
    new Function(`async () => { new.target }`);
    new Function(`async () => await new.target`);
    new Function(`async () => { await new.target }`);

    function f() { () => new.target };
    function f() { () => { new.target } };
    function f() { async () => new.target };
    function f() { async () => { new.target } };
    function f() { async () => await new.target };
    function f() { async () => { await new.target } };

    (function() { eval(`() => new.target`) })();
    (function() { eval(`() => { new.target }`) })();
    (function() { eval(`async () => new.target`) })();
    (function() { eval(`async () => { new.target }`) })();
    (function() { eval(`async () => await new.target`) })();
    (function() { eval(`async () => { await new.target }`) })();
} catch (e) {
    sawError = true;
}
assert(!sawError);
