function shouldThrow(run, errorType, message) {
    let actual;
    var hadError = false;

    try {
        actual = run();
    } catch (e) {
        hadError = true;
        actual = e;
    }

    if (!hadError)
        throw new Error("Expected " + run + "() to throw " + errorType.name + ", but did not throw.");
    if (!(actual instanceof errorType))
        throw new Error("Expeced " + run + "() to throw " + errorType.name + " , but threw '" + actual + "'");
    if (message !== void 0 && actual.message !== message)
        throw new Error("Expected " + run + "() to throw '" + message + "', but threw '" + actual.message + "'");
}

shouldThrow(() => {
    String.prototype.toString.call(null);
}, TypeError, "String.prototype.toString requires that |this| be a string");

shouldThrow(() => {
    String.prototype[Symbol.iterator].call(null);
}, TypeError, "String.prototype[Symbol.iterator] requires that |this| not be null or undefined");

const prototypeFunctionNames = [
    "charAt",
    "charCodeAt",
    "codePointAt",
    "indexOf",
    "lastIndexOf",
    "slice",
    "substr",
    "substring",
    "toLowerCase",
    "toUpperCase",
    "toLocaleLowerCase",
    "toLocaleUpperCase",
    "startsWith",
    "endsWith",
    "includes",
    "normalize",
    "isWellFormed",
    "toWellFormed",
    "at"
];
for (const prototypeFunctionName of prototypeFunctionNames) {
    shouldThrow(() => {
        String.prototype[prototypeFunctionName].call(null);
    }, TypeError, `String.prototype.${prototypeFunctionName} requires that |this| not be null or undefined`);
}
