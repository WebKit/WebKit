const child = new Worker("idle-worker.js");
child.onmessage = () => postMessage("ready");
child.onerror = event => { throw new Error(event.message); };
