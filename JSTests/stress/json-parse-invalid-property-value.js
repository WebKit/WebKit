function shouldThrowSyntaxError(source, expectedMessage) {
    let error;
    try {
        JSON.parse(source);
    } catch (e) {
        error = e;
    }
    if (!(error instanceof SyntaxError))
        throw new Error(`Expected SyntaxError for ${source}, got ${error}`);
    if (error.message !== expectedMessage)
        throw new Error(`Bad message for ${source}: ${error.message}`);
}

for (let i = 0; i < 3; ++i) {
    shouldThrowSyntaxError(`{"a":-}`, "JSON Parse error: Invalid number");
    shouldThrowSyntaxError(`{"a":1.}`, "JSON Parse error: Invalid digits after decimal point");
    shouldThrowSyntaxError(`{"a":1.e5}`, "JSON Parse error: Invalid digits after decimal point");
    shouldThrowSyntaxError(`{"a":"\\x"}`, "JSON Parse error: Invalid escape character x");
    shouldThrowSyntaxError(`{"a":"abc`, "JSON Parse error: Unterminated string");
    shouldThrowSyntaxError(`{"a":"\t"}`, "JSON Parse error: Unterminated string");
    shouldThrowSyntaxError(`{"a":1,"b":-}`, "JSON Parse error: Invalid number");
    shouldThrowSyntaxError(`{"a":1,"b":"\\u12"}`, "JSON Parse error: \"\\u12\"}\" is not a valid unicode escape");
}
