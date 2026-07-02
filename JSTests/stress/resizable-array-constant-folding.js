//@ runDefault("--useConcurrentJIT=false")
const memories = [];

(function() {
    while (true) {
        try {
            memories.push(new WebAssembly.Memory({initial: 65535, maximum: 65536}));
        } catch (e) {
            break;
        }
    }
})();

const memory = new WebAssembly.Memory({initial: 1, maximum: 100});
const buffer = memory.toResizableBuffer();
const view = new Float64Array(buffer);

function trigger(val) {
    view[0] = val;
}

for (let i = 0; i < 10000; i++) {
    trigger(13.37);
}

memory.grow(1);
trigger(1.1);
