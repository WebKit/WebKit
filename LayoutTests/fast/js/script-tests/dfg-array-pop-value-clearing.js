description(
"Tests that array popping properly cleans up the popped element."
);

function foo(a) {
    var x = a.pop();
    a[a.length + 1] = 42;
    return [x, a.pop(), a.pop()];
}

for (var i = 0; i < 1000; ++i)
    shouldBe("foo([1, 2])", "[2,42,,]");
