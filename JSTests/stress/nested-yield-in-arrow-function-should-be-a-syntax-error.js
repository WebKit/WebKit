let passed = false;
try {
    new Function("\nfor (let a of (function*() { \n       for (var b of (function*() { \n               for (var c of (function*() { \n                       for (var d of (function*() {\n                               for (var e of (function*() { \n                                       for (var f of (function*() {\n                                               for (var g of (x = (yield * 2)) => (1)) {\n                                               }\n                                       })()) {\n                                       }\n                               })()) {\n                               }\n                       })()) {\n                       }\n               })()) {\n               }\n       })()) {\n       }\n})()) {\n}\n");
} catch (e) {
    if (e instanceof SyntaxError)
        passed = true;
} finally {
    if (passed !== true)
        throw new Error("Test did not throw a Syntax Error as expected");
}
