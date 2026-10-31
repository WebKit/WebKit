const child = new Worker("worker-wasm-atomic-wait-forever.js");
child.onmessage = () => postMessage("ready");
child.onerror = event => { throw new Error(event.message); };
