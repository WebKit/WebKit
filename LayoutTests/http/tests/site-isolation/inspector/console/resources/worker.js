console.log(`Worker started`);

self.addEventListener('message', (event) => {
    console.log(`Message received in the worker: ${event.data}`);
});
