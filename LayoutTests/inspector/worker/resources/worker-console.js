onmessage = function(event) {
    switch (event.data.method) {
    case "log":
        console.log("log!", [self, self.location, 123, Symbol()]);
        break;
    case "warn":
        console.warn("warning!");
        break;
    case "error":
        console.error("error!");
        break;
    case "assert":
        console.assert(true, "SHOULD NOT SEE THIS");
        console.assert(false, "Assertion Failure");
        break;
    case "time":
        console.time("name");
        console.timeEnd("name");
        break;
    case "count":
        console.count();
        break;
    case "screenshot":
        switch (event.data.type) {
        case "ImageBitmap": {
            // 2x2 red square
            fetch("data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAIAAAACCAYAAABytg0kAAAAAXNSR0IArs4c6QAAABNJREFUCB1j/M/AAEQMDEwgAgQAHxcCAmtAm/sAAAAASUVORK5CYII=")
                .then((response) => response.blob())
                .then((blob) => createImageBitmap(blob))
                .then((imageBitmap) => {
                    console.screenshot(imageBitmap);
                });
            break;
        }
        case "ImageData":
            console.screenshot(new ImageData(2, 2))
            break;
        case "OffscreenCanvas": {
            let canvas = new OffscreenCanvas(2, 2);
            let context = canvas.getContext("2d");
            context.fillStyle = "red";
            context.fillRect(0, 0, 2, 2);
            console.screenshot(canvas);
            break;
        }
        case "OffscreenCanvasRenderingContext2D": {
            let canvas = new OffscreenCanvas(2, 2);
            let context = canvas.getContext("2d");
            console.screenshot(context);
            break;
        }
        case "primitive":
            console.screenshot(...event.data.args);
            break;
        }
        break;
    }
}
