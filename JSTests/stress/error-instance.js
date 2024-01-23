const NUM_NESTED_FUNCTIONS = 100;

function main() {
    const error = (new Function(`return (function () { arguments.callee.displayName = 'a'.repeat(0x100000) + 'b'; `.repeat(NUM_NESTED_FUNCTIONS) + `return new Error();` + ` })();`.repeat(NUM_NESTED_FUNCTIONS)))();
    
    error.stack;
}

main();
