onmessage = function(event) {
    let ctx = event.data.canvas.getContext('2d');
    ctx.fillStyle = "lime";
    ctx.fillRect(0, 75, 150, 150);
    ctx.fillStyle = "yellow";
    ctx.fillRect(150, 0, 300, 75);
    // Ensure that offscreen canvas has submitted the frame to the placeholder by ensuring
    // "update rendering" phase of Worker lifecycle has executed few times.
    requestAnimationFrame(() => {
        requestAnimationFrame(() => {
            self.postMessage("done");
        });
    });
};
