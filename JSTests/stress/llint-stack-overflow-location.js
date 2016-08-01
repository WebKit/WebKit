//@ runNoJIT

function stackTraceDescription(stackFrame) {
    let indexOfAt = stackFrame.indexOf('@')
    let indexOfLastSlash = stackFrame.lastIndexOf('/');
    if (indexOfLastSlash == -1)
        indexOfLastSlash = indexOfAt
    let functionName = stackFrame.substring(0, indexOfAt);
    let fileName = stackFrame.substring(indexOfLastSlash + 1);
    return functionName + " at " + fileName;
}

function foo(j) {
    for (let i = 0; i < 20; i++) {
        i--;
        i++;
    }
    foo(j + 1);
}

let error = null;
try {
    foo(10);
} catch(e) {
    error = e; 
}

if (!error)
    throw new Error("No exception!");

let frame = error.stack.split("\n")[0];
let description = stackTraceDescription(frame);
if (description.indexOf(".js:18") < 0)
    throw new Error("Bad location: '" + description + "'");

