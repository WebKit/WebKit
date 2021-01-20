//@ requireOptions("--forceEagerCompilation=true", "--useConcurrentJIT=false")

function main() {
    const foo = {x: 0};
    foo.__proto__ = new Proxy({}, { ownKeys() { return []; } });
    for (const x in foo) { }
}

for (let i = 0; i < 0x1000; i++)
    main();
