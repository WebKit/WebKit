var exception;

try {
    eval("'\\\r\n'");
} catch (e) {
    exception = e;
}

if (exception)
    throw "FAILED: \\r\\n should be handled as a line terminator";

try {
    eval("'\\\n\r'");
} catch (e) {
    exception = e;
}

if (exception != "SyntaxError: Unexpected EOF")
    throw "FAILED: \\n\\r should NOT be handled as a line terminator.  Expected exception: 'SyntaxError: Unexpected EOF', actual: '" + exception + "'";
