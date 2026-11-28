let started = true;

onmessage = (event) => {
    postMessage("reply: " + event.data);
};
