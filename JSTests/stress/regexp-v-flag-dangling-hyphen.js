function shouldThrowSyntaxError(source) {
    let error = null;
    try {
        new RegExp(source, "v");
    } catch (e) {
        error = e;
    }
    if (!(error instanceof SyntaxError))
        throw new Error(`Expected SyntaxError for /${source}/v but got ${error}`);
}

function shouldNotThrow(source) {
    new RegExp(source, "v");
}

// In /v mode, '-' is a ClassSetSyntaxCharacter and is only legal between two
// ClassSetCharacters as part of a ClassSetRange. A bare or trailing '-' with
// no right-hand side must be rejected.
shouldThrowSyntaxError("[a-]");
shouldThrowSyntaxError("[\\d-]");
shouldThrowSyntaxError("[\\w-]");
shouldThrowSyntaxError("[a-z\\d-]");
shouldThrowSyntaxError("[\\p{ASCII}-]");
shouldThrowSyntaxError("[a-[b]]");
shouldThrowSyntaxError("[\\d-[b]]");

// These should still parse.
shouldNotThrow("[a-z]");
shouldNotThrow("[a\\-]");
shouldNotThrow("[\\-a]");
shouldNotThrow("[a--b]");
shouldNotThrow("[a&&b]");
shouldNotThrow("[\\w--\\d]");
shouldNotThrow("[\\d\\-a]");
