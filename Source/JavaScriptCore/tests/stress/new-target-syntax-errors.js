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
