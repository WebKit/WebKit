description(
"This test checks that non-numeric sort functions always have JIT code. This test passes if it does not crash."
);

function f()
{
}

[].sort(f);

function g()
{
}

function h(x)
{
    x();
}

h(g);
h(g);
h(g);
h(f);
