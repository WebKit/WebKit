var callCount = 0;

function putInGlobal(key, val) {
    callCount++;
    globalThis.key = val;
}

function getFromGlobal(key) {
    callCount++;
    return globalThis.key;
}

var anObject = new Object();

function getAnObject() {
    callCount++;
    return anObject;
}

function getCallCount() {
    return callCount;
}

var answer = 6;
export { anObject, answer, getCallCount, getFromGlobal, getAnObject, putInGlobal };
