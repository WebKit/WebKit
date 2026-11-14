load("./resources/for-in-random-objects.js", "caller relative");

test(100, () => ({ anything: true, during: 4, anythingLater: true }));
