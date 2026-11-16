// Result reporter for running the imported WPT WebAssembly tests in the `jsc` shell under
// run-jsc-stress-tests. Loaded last of the pre/mid/post harness shims (see wpt-harness-pre.js for
// the load order), after the test file.
//
// In a browser these results are emitted by resources/testharnessreport.js into the DOM and
// dumped as text; that path needs testRunner/document, which the shell lacks. This reproduces
// the same text so stdout can be diffed against the committed -expected.txt baselines.
//
// wpt-harness-mid.js has already called setup({ explicit_done: true }), so the harness will not
// finalize until done() is called below. Registering the completion callback first guarantees it
// is in place for the results; done() then releases the wait, and completion fires once the
// promise/timer chain drains via deferredWorkTimer->runRunLoop() before exit.

// Title passed by the runner as a trailing `-- <title> <baseURL>` argument pair. The browser names
// an unnamed block-body test after the page title; the shell's ShellTestEnvironment hardcodes
// "Untitled"/"Untitled N" instead (that environment object is a closure local, so it cannot be
// patched from here). Remapping the "Untitled" prefix to the title reproduces the committed
// baseline. Single-line arrow tests derive their name from the function source in both
// environments, so they are unaffected.
const WPT_TITLE = (globalThis.arguments && globalThis.arguments.length > 0) ? globalThis.arguments[0] : null;

// URL the WPT server serves this test's directory from. The WAST harness bakes a stack string into
// each assertion description, and a frame's location is the URL the browser loaded the script from
// but a bare relative load path in the shell. Prefixing each frame with this reproduces the URLs in
// the committed baselines.
const WPT_BASE_URL = (globalThis.arguments && globalThis.arguments.length > 1) ? globalThis.arguments[1] : null;

function titledName(name) {
    if (WPT_TITLE && /^Untitled( \d+)?$/.test(name))
        return name.replace(/^Untitled/, () => WPT_TITLE);
    return name;
}

// Rewrites the `name@path:line:column` frames of an assertion description so each path becomes the
// URL the WPT server would serve it from.
function urlifyStackFrames(message) {
    if (!WPT_BASE_URL)
        return message;
    return message.replace(/@(?=[A-Za-z0-9_./-]+\.js:\d+:\d+)/g, () => "@" + WPT_BASE_URL);
}

function convertResult(status) {
    if (status == 0) return "PASS";
    if (status == 1) return "FAIL";
    if (status == 2) return "TIMEOUT";
    return "NOTRUN";
}

function sanitize(text) {
    if (!text)
        return "";
    return text.replace(/\0/g, "\\0").replace(/\r/g, "\\r");
}

add_completion_callback(function (tests, harness_status) {
    let out = "\n";
    if (harness_status.status != 0)
        out += "Harness Error (" + convertResult(harness_status.status) + "), message = " +
               harness_status.message + "\n\n";

    for (const test of tests) {
        let message = sanitize(test.message);
        // Match the browser baseline: drop the stack from failed subtests so the text is
        // stable across environments (testharnessreport.js does the same unless dumpStack).
        if (test.status == 1 && !test.dumpStack) {
            const stackIndex = message.indexOf("(stack:");
            if (stackIndex > 0)
                message = message.substr(0, stackIndex);
        }
        // Append the message only when present; the committed baselines have no trailing
        // space after the name for passing subtests, and diff does not ignore trailing spaces.
        out += convertResult(test.status) + " " + titledName(sanitize(test.name)) + (message ? " " + urlifyStackFrames(message) : "") + "\n";
    }

    print(out); // print() appends the final newline, reproducing the trailing blank line.
});

// Release the explicit_done wait that wpt-harness-mid.js set; completion (and the callback above)
// fires once the test's promise/timer chain drains.
done();

