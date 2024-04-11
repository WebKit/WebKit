onmessage = function(event) {
    console.log(event)
    console.log(event.data)
    console.log(event.data.canvas)
    let gl = event.data.canvas.getContext('webgl');
    gl.enable(gl.SCISSOR_TEST);
    gl.scissor(0, 0, 150, 75);
    gl.clearColor(0, 1, 0, 1);
    gl.clear(gl.COLOR_BUFFER_BIT);
    gl.scissor(150, 75, 150, 75);
    gl.clearColor(1, 1, 0, 1);
    gl.clear(gl.COLOR_BUFFER_BIT);
    if (typeof window == "undefined")
        self.postMessage("done");
};
