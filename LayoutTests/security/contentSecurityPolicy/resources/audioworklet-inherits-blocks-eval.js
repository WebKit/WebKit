class EvalTestProcessor extends AudioWorkletProcessor {
    constructor() {
        super();
        var exception;
        try {
            eval("1 + 0");
        } catch (e) {
            exception = e;
        }
        if (!exception)
            this.port.postMessage("FAIL should throw EvalError. But did not throw an exception.");
        else if (exception instanceof EvalError)
            this.port.postMessage("PASS threw exception " + exception + ".");
        else
            this.port.postMessage("FAIL should throw EvalError. Threw exception " + exception + ".");
    }
    process() { return false; }
}
registerProcessor('eval-test', EvalTestProcessor);
