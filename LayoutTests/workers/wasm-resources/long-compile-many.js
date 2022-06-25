const numWorkers = 20;
let count = 0;
function done() {
    if (++count === 2*numWorkers) {
        console.log("Finished test");
        if (window.testRunner)
            testRunner.notifyDone();
    }
}

for (let i = 0; i < numWorkers; ++i) {
    let worker = new Worker("./wasm-resources/long-compile-worker.js");
    worker.onmessage = function(e) {
        if (e.data === "done") {
            done();
            return;
        }

        if (!(e.data instanceof WebAssembly.Module)) {
            throw new Error("Bad post message");
        }

        async function run(module) {
            let instance = await WebAssembly.instantiate(module);
            const count = 7000;
            if (instance.exports.f1(i) !== (i*count + i*2))
                console.log("Bad result");
            else
                console.log("Good result");
            done();
        }
        run(e.data);
    }
}
