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

checkSyntax(`()=>42`);
checkSyntax(`()=>42
`);
checkSyntax(`()=>42//Hello`);
checkSyntax(`()=>42//Hello
`);
