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
            this.port.postMessage("PASS eval() was allowed by CSP.");
        else
            this.port.postMessage("FAIL eval() threw exception " + exception + ".");
    }
    process() { return false; }
}
registerProcessor('eval-test', EvalTestProcessor);
