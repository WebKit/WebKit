//@ skip
//@ requireOptions("--watchdog=100")

// FIMXE: skipping this test for now because it takes too long to run until we have a fix
// for https://bugs.webkit.org/show_bug.cgi?id=191855.

for (let i=0; i<1000; i++)
    import(0);
