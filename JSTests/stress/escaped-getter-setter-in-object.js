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
        throw new Error(`Bad error: ${String(error)}`);
}

testSyntaxError(String.raw`
({
    \u0067\u0065\u0074 m() {}
});
`, `SyntaxError: Unexpected identifier 'm'. Expected a ':' following the property name 'get'.`);

testSyntaxError(String.raw`
({
    \u0073\u0065\u0074 m(v) {}
});
`, `SyntaxError: Unexpected identifier 'm'. Expected a ':' following the property name 'set'.`);
