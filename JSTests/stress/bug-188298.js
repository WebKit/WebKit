// This test passes if it does not crash.

function foo() {
    if (1 < 2);
    while (true) {
        if (1 < 2) break;
    }
}

for (var i = 0; i < 10000; i++)
    foo();

