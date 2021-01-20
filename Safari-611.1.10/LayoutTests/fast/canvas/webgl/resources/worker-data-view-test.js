onmessage = function(evt) {
    var array = new Uint32Array(1);
    var view = new DataView(array.buffer);
    view.setUint32(0, 12345678, true);
    postMessage(view.getUint32(0, true));
}
