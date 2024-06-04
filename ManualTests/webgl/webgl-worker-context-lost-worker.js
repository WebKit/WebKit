self.addEventListener("message", (e) => {
    const oc = e.data.oc;
    console.log("worker support WebGL: " + oc);
    let gl = oc.getContext('webgl');
    oc.addEventListener("webglcontextlost", (event) => {
        console.log("context lost");
        event.preventDefault();
    });
    oc.addEventListener("webglcontextrestored", (event) => {
        console.log("context restored");
        gl.clearColor(0, 1, 0, 1);
        gl.clear(gl.COLOR_BUFFER_BIT);
    });
    gl.clearColor(1, 0, 0, 1);
    gl.clear(gl.COLOR_BUFFER_BIT);
});
