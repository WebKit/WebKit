load("./resources/for-in-random-objects.js", "caller relative");

test(30, () => ({ anything: true, during: 4, anythingLater: true, whileGettingNames: true }));
