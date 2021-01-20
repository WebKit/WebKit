var exception;
function* items() {
    yield { };
    yield* 42;
}

try {
    for (let i of items()) { }
} catch (e) {
    exception = e;
}

if (exception != "TypeError: undefined is not a function (near '...yield* 42...')")
    throw FAILED;
