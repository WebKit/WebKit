let startTime = Date.now();
let waitTime = 1000;

setTimeout(() => {
    if (startTime + waitTime > Date.now())
        throw new Error();
}, waitTime);