function f() {
    foo = f.caller;
}

// Test C++ code constructor
new Number({ valueOf: f });

if (foo !== null)
    throw new Error(foo);

foo = 1;

// Test C++ function.
[1].slice({ valueOf: f });

if (foo !== null)
    throw new Error(foo);

foo = 1;

// Test builtin js code
[1].map(f)

if (foo !== null)
    throw new Error(foo);
