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

testSyntax(`try { } catch { }`);
testSyntax(`try { } catch { } finally { }`);
testSyntaxError(`try { } catch { { }`, `SyntaxError: Unexpected end of script`);
testSyntaxError(`try { } catch () { }`, `SyntaxError: Unexpected token ')'. Expected a parameter pattern or a ')' in parameter list.`);
testSyntaxError(`try { } catch }`, `SyntaxError: Unexpected token '}'. Expected '(' to start a 'catch' target.`);
testSyntaxError(`try { } catch {`, `SyntaxError: Unexpected end of script`);
testSyntaxError(`try { } catch {`, `SyntaxError: Unexpected end of script`);
