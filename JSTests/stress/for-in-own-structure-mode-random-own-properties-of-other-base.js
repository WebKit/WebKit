load("./resources/for-in-random-objects.js", "caller relative");

test(100, () => ({ other: true, during: 4 }));
