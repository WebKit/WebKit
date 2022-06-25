var exception;
try {
    eval("1 + 0");
} catch (e) {
    exception = e;
}
if (!exception)
    self.postMessage("FAIL should throw EvalError. But did not throw an exception.");
else {
    if (exception instanceof EvalError)
        self.postMessage("PASS threw exception " + exception + ".");
    else
        self.postMessage("FAIL should throw EvalError. Threw exception " + exception + ".");
}
