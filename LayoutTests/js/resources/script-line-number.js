description(
"This test checks that line numbers are correctly reported for both inline scripts and inline event handlers."
);

function getLineFromError(e)
{
    // JSC
    if (e.line)
        return e.line;

    // V8
    if (e.stack) {
        // ErrorName: ErrorDescription at FileName:LineNumber:ColumnNumber
        parts = e.stack.split(":");
        return parts[parts.length - 2];
    }

    return -1;
}

function assertErrorOnLine(error, expectedLine)
{
    shouldBe(stringify(getLineFromError(error)), stringify(expectedLine));
}
