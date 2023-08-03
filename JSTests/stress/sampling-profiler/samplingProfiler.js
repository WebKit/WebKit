if (!platformSupportsSamplingProfiler())
    throw new Error("Sampling profiler not supported");

startSamplingProfiler();

function assert(b) {
    if (!b)
        throw new Error("bad stack trace")
}

let nodePrototype = {
    makeChildIfNeeded: function(name) {
        if (!this.children[name])
            this.children[name] = makeNode(name);
        return this.children[name];
    }
};

function makeNode(name) {
    let node = Object.create(nodePrototype);
    node.name = name;
    node.children = {};
    return node
}

function updateCallingContextTree(root) {
    let stacktraces = samplingProfilerStackTraces();
    for (let stackTrace of stacktraces.traces) {
        let node = root;
        for (let i = stackTrace.frames.length; i--; ) {
            let functionName = stackTrace.frames[i].name;
            node = node.makeChildIfNeeded(functionName);
        }
    }
}

const VERBOSE = false;

function doesTreeHaveStackTrace(tree, stackTraceOrString, isRunFromRunTest = true) {
    if (typeof stackTraceOrString === 'string') {
        // Let's ensure that this signature exists in the stack trace.
        function check(node) {
            for (let name of Object.keys(node.children)) {
                if (name === stackTraceOrString)
                    return true;
                else {
                    if (check(node.children[name]))
                        return true;
                }
            }
            return false;
        }
        if (!check(tree)) {
            if (VERBOSE) {
                print("failing");
                print(JSON.stringify(tree));
            }
            return false;
        }
        return true;
    }

    // stack trace should be top-down array with the deepest
    // call frame at index 0.
    let stackTrace = null;
    if (isRunFromRunTest)
        stackTrace = [...stackTraceOrString, "runTest", "(program)"];
    else
        stackTrace = [...stackTraceOrString];
    
    let node = tree;
    for (let i = stackTrace.length; i--; ) {
        let prev = node;
        node = node.children[stackTrace[i]];
        if (!node) {
            if (VERBOSE) {
                print("failing on " + i + " : " + stackTrace[i]);
                print(JSON.stringify(tree));
                print(Object.keys(prev.children));
            }
            return false;
        }
    }
    return true;
}

function makeTree() {
    let root = makeNode("<root>");
    updateCallingContextTree(root);
    return root;
}

// This test suite assumes that "runTest" is being called
// from the global scope.
function runTest(func, stackTraceOrString) {
    const timeToFail = 50000;
    let startTime = Date.now();
    let root = makeNode("<root>");
    do {
        for (let i = 0; i < 100; i++) {
            for (let i = 0; i < 10; i++) {
                func();
            }
            updateCallingContextTree(root);
            if (doesTreeHaveStackTrace(root, stackTraceOrString)) {
                if (VERBOSE)
                    print(`Time to finish: ${Date.now() - startTime}`);
                return;
            }
        }
    } while (Date.now() - startTime < timeToFail);
    print(JSON.stringify(root, undefined, 2));
    doesTreeHaveStackTrace(root, stackTraceOrString, true, true);
    throw new Error("Bad stack trace");
}

function dumpTree(tree) {
    print(JSON.stringify(tree, undefined, 2));
}
