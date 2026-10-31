const child = new Worker("nested-worker.js");
child.onmessage = () => postMessage("ready");
child.onerror = event => { throw new Error(event.message); };
