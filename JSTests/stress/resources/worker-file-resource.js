onmessage = function (event) {
    postMessage("file worker got " + event.data);
    close();
};
