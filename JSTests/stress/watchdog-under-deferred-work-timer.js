//@ requireOptions("--watchdog=300", "--watchdog-exception-ok")

function iterate() {
    while (true) {
        1 + 1;
    }
}

setTimeout(iterate, 0);
