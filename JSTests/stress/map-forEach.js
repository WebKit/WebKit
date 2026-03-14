//@skip if $memoryLimited
function startScan() {
    for (let i = 0; i < 5; i++) {
        new WebAssembly.Memory({
            initial: 256
        });
    }
}

function main() {
    const object = {
        key: 1
    };

    let map = new Map();
    map.set({0: 1.1}, 1);

    function inlinee(value, key) {
        object.key = key;
    }

    for (let i = 0; i < 200; i++) {
       map.forEach(inlinee);
    }

    for (let i = 0; i < 100; i++) {
        startScan();

        (new Map([[{0: 1.1}, 1]])).forEach(inlinee);

        const spray = [];
        for (let j = 0; j < 1000; j++) {
            spray.push({0: 2.2});
        }

        if (object.key[0] === 2.2) {
            throw new Error("bad");
        }
    }
}

main();
