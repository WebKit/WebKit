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
    for (let stackTrace of stacktraces) {
        let node = root;
        for (let i = stackTrace.length; i--; ) {
            let functionName = stackTrace[i];
            node = node.makeChildIfNeeded(functionName);
        }
    }
}

function doesTreeHaveStackTrace(tree, stackTrace, isRunFromRunTest = true, verbose = false) {
    // stack trace should be top-down array with the deepest
    // call frame at index 0.
    if (isRunFromRunTest)
        stackTrace = [...stackTrace, "runTest", "<global>"];
    else
        stackTrace = [...stackTrace];
    
    let node = tree;
    for (let i = stackTrace.length; i--; ) {
        node = node.children[stackTrace[i]];
        if (!node) {
            if (verbose)
                print("failing on " + i + " : " + stackTrace[i]);
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

const VERBOSE = false;
// This test suite assumes that "runTest" is being called
// from the global scope.
function runTest(func, stackTrace) {
    const timeToFail = 50000;
    let startTime = Date.now();
    let root = makeNode("<root>");
    do {
        for (let i = 0; i < 100; i++) {
            for (let i = 0; i < 10; i++) {
                func();
            }
            updateCallingContextTree(root);
            if (doesTreeHaveStackTrace(root, stackTrace)) {
                if (VERBOSE)
                    print(`Time to finish: ${Date.now() - startTime}`);
                return;
            }
        }
    } while (Date.now() - startTime < timeToFail);
    print(JSON.stringify(root, undefined, 2));
    doesTreeHaveStackTrace(root, stackTrace, true, true);
    throw new Error("Bad stack trace");
}

function dumpTree(tree) {
    print(JSON.stringify(tree, undefined, 2));
}
