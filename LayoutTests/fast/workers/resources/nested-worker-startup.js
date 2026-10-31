const child = new Worker("worker-run-forever.js");
child.onerror = event => { throw new Error(event.message); };
