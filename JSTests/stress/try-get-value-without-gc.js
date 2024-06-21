//@ runDefault("--watchdog=1000", "--watchdog-exception-ok")
function main() {
    error = (new Function(`return (function () { arguments.callee.displayName = 'a'.repeat(0x100000) + 'b'; `.repeat(100) + `return new Error();` + ` })();`.repeat(100)))();
    main.apply();
}
main();
