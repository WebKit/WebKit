description(
"Tests that if array allocation profiling causes a new_array to allocate double arrays, then the holes end up being correctly initialized."
);

function foo(array, i) {
    return array[i];
}

for (var i = 0; i < 100; ++i)
    shouldBe("foo([, 1.5], 0)", "void 0");

