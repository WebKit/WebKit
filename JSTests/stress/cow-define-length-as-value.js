function test(create) {
    // Set length to be smaller.
    Object.defineProperty(create(), "length", { value: 1 });

    // Set length to be bigger.
    Object.defineProperty(create(), "length", { value: 4 });

    // Set length to be the same size
    Object.defineProperty(create(), "length", { value: 3 });
}

// Test Int32.
test(() => [1, 2]);
// Test double
test(() => [1.123, 2.50934]);
// Test contiguous via NaN
test(() => [NaN, 2.50934]);
// Test contiguous via string
test(() => ["test", "42"]);
