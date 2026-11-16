onmessage = function (message) {
    var memory = message.data;
    var view = new Uint8Array(memory.buffer);
    view[0] = 43;
    view[1] = 42;
    memory.grow(1);
    view[2] = 44;
    self.postMessage("DONE");
}
