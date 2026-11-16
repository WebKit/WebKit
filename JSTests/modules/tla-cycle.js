try {
    await import("./tla-cycle/a.js");
    throw "Expected an exception";
} catch (e) {
    if (e.message !== "C failed") {
        throw "Incorrect exception message";
    }
}
