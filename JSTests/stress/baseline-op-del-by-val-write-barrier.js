//@ requireOptions("--verifyHeap=1", "--useConcurrentJIT=0")

function main() {
    globalThis.dummy = {x: 1};
    globalThis.dummy.y = 2;

    for (let i = 0; i < 100; i++) {
        const target = {x: 1};
        fullGC();

        let opt = new Function('o', 'o = delete o["x"];');

        for (let j = 0; j < 10; j++) {
            opt({x: 1});
        }

        opt(target);
        opt = null;

        edenGC();
    }
}

main();
