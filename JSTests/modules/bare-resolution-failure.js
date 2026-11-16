let e1, e2;

try {
    await import("./bare-resolution-failure/a/x.js");
} catch (e) {
    e1 = e;
}

try {
    await import("./bare-resolution-failure/b/y.js");
} catch (e) {
    e2 = e;
}

if (!e1 || !e2) {
    throw new Error("Both imports should fail");
}

if (e1 === e2) {
    throw new Error("Exceptions shouldn't compare equal");
}

if (e1.message === e2.message) {
    throw new Error("Exception messages should be different");
}
