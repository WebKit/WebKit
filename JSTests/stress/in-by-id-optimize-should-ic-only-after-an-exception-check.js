//@ runDefault("--watchdog=100", "--watchdog-exception-ok")
function foo() {
    function bar() {}
    'prototype' in bar;
}

for (let i = 0; i < 1_000_000; i++)
    foo();
