function assert(x) {
    if (!x)
        throw new Error("Bad assertion!");
}

function shouldThrow(func, expectedError) {
    let errorThrown = false;
    try {
        func();
    } catch (error) {
        errorThrown = true;
        if (error.toString() !== expectedError)
            throw new Error(`Bad error: ${error}!`);
    }
    if (!errorThrown)
        throw new Error("Didn't throw!");
}


eval("var theVar");
shouldThrow(() => { $262.evalScript("let theVar = 1"); }, "SyntaxError: Can't create duplicate variable: 'theVar'");
shouldThrow(() => { $262.evalScript("const theVar = 1"); }, "SyntaxError: Can't create duplicate variable: 'theVar'");
delete globalThis.theVar;
$262.evalScript("let theVar = 2");
assert(theVar === 2);


eval("function theTopLevelFunctionDeclaration() {}");
shouldThrow(() => { $262.evalScript("let theTopLevelFunctionDeclaration = 3"); }, "SyntaxError: Can't create duplicate variable: 'theTopLevelFunctionDeclaration'");
shouldThrow(() => { $262.evalScript("const theTopLevelFunctionDeclaration = 3"); }, "SyntaxError: Can't create duplicate variable: 'theTopLevelFunctionDeclaration'");
delete globalThis.theTopLevelFunctionDeclaration;
$262.evalScript("const theTopLevelFunctionDeclaration = 4");
assert(theTopLevelFunctionDeclaration === 4);


eval("if (true) { function theHoistedBlockLevelFunctionDeclaration() {} }");
shouldThrow(() => { $262.evalScript("const theHoistedBlockLevelFunctionDeclaration = 5"); }, "SyntaxError: Can't create duplicate variable: 'theHoistedBlockLevelFunctionDeclaration'");
shouldThrow(() => { $262.evalScript("let theHoistedBlockLevelFunctionDeclaration = 5"); }, "SyntaxError: Can't create duplicate variable: 'theHoistedBlockLevelFunctionDeclaration'");
delete globalThis.theHoistedBlockLevelFunctionDeclaration;
$262.evalScript("let theHoistedBlockLevelFunctionDeclaration = 6");
assert(theHoistedBlockLevelFunctionDeclaration === 6);
