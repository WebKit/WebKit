(function test() {
    let e = new Error();
    Error.captureStackTrace(e, test);
    let firstStack = e.stack;
    Error.captureStackTrace(e, test);
    let secondStack = e.stack;
    if (firstStack !== secondStack)
       throw new Error("captureStackTrace not idempotent on Error before materialization");
})();
