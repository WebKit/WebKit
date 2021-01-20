description('Test for <a href="https://bugs.webkit.org/show_bug.cgi?id=33641">bug 33641</a>: Assertion failure in Lexer.cpp if input stream ends while in string escape.');

debug("Passed if no assertion failure.");

try {
    eval('"\\');
} catch (ex) {
}
