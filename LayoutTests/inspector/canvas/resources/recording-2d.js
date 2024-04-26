let ctx = null;

// 2x2 red square
let image = document.createElement("img");
image.src = "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAIAAAACCAYAAABytg0kAAAAAXNSR0IArs4c6QAAABNJREFUCB1j/M/AAEQMDEwgAgQAHxcCAmtAm/sAAAAASUVORK5CYII=";

// Invalid video
let video = document.createElement("video");

// Blank canvas
let canvas = document.createElement("canvas");
canvas.width = 2;
canvas.height = 2;

let linearGradient = null;

let radialGradient = null;

let pattern = null;

let path12 = new Path2D("M 1 2");
let path34 = new Path2D("M 3 4");

let imageData14 = new ImageData(1, 4);
let imageData23 = new ImageData(2, 3);

let bitmap = null;

async function load({offscreen} = { }) {
    if (offscreen) {
        if (window.OffscreenCanvas)
            ctx = new OffscreenCanvas(2, 2).getContext("2d");
    } else
        ctx = canvas.getContext("2d");

    if (ctx) {
        linearGradient = ctx.createLinearGradient(1, 2, 3, 4);
        radialGradient = ctx.createRadialGradient(1, 2, 3, 4, 5, 6);
        pattern = ctx.createPattern(image, "no-repeat");
        bitmap = await createImageBitmap(image);

        ctx.save();
        cancelActions();
    }

    runTest();
}

function ignoreException(func){
    try {
        func();
    } catch (e) { }
}

let requestAnimationFrameId = NaN;
let saveCount = 1;

function cancelActions() {
    for (let i = 0; i < saveCount; ++i)
        ctx.restore();
    ctx.restore(); // Ensures the state is reset between test cases.

    cancelAnimationFrame(requestAnimationFrameId);
    requestAnimationFrameId = NaN;

    ctx.save(); // Ensures the state is reset between test cases.
    ctx.save(); // This matches the `restore` call in `performActions`.
    saveCount = 1;

    ctx.resetTransform();
    ctx.beginPath();
    ctx.clearRect(0, 0, ctx.canvas.width, ctx.canvas.height);
}

function performActions() {
    let frames = [
        () => {
            ignoreException(() => ctx.arc(1, 2, 3, 4, 5));
            ignoreException(() => ctx.arc(6, 7, 8, 9, 10, true));
        },
        () => {
            ignoreException(() => ctx.arcTo(1, 2, 3, 4, 5));
        },
        () => {
            ctx.beginPath();
        },
        () => {
            ctx.bezierCurveTo(1, 2, 3, 4, 5, 6);
        },
        () => {
            ctx.clearRect(1, 2, 3, 4);
        },
        () => {
            ctx.clearShadow?.();
        },
        () => {
            ctx.clip();
            ctx.clip("evenodd");
            ctx.clip(path12);
            ctx.clip(path34, "evenodd");
        },
        () => {
            ctx.closePath();
        },
        () => {
            ignoreException(() => ctx.createImageData(imageData14));
            ignoreException(() => ctx.createImageData(2, 3));
        },
        () => {
            ignoreException(() => ctx.createLinearGradient(1, 2, 3, 4));
        },
        () => {
            ignoreException(() => ctx.createPattern(image, "testA"));
            ignoreException(() => ctx.createPattern(video, "testB"));
            ignoreException(() => ctx.createPattern(canvas, "testC"));
            ignoreException(() => ctx.createPattern(bitmap, "testD"));
        },
        () => {
            ignoreException(() => ctx.createRadialGradient(1, 2, 3, 4, 5, 6));
        },
        () => {
            ctx.direction;
            ctx.direction = "test";
        },
        () => {
            ctx.drawFocusIfNeeded?.(document.body);
            ctx.drawFocusIfNeeded?.(path12, document.body);
        },
        () => {
            ignoreException(() => ctx.drawImage(image, 11, 12));
            ignoreException(() => ctx.drawImage(image, 13, 14, 15, 16));
            ignoreException(() => ctx.drawImage(image, 17, 18, 19, 110, 111, 112, 113, 114));

            ignoreException(() => ctx.drawImage(video, 21, 22));
            ignoreException(() => ctx.drawImage(video, 23, 24, 25, 26));
            ignoreException(() => ctx.drawImage(video, 27, 28, 29, 210, 211, 212, 213, 214));

            ignoreException(() => ctx.drawImage(canvas, 31, 32));
            ignoreException(() => ctx.drawImage(canvas, 33, 34, 35, 36));
            ignoreException(() => ctx.drawImage(canvas, 37, 38, 39, 310, 311, 312, 313, 314));

            ignoreException(() => ctx.drawImage(bitmap, 41, 42));
            ignoreException(() => ctx.drawImage(bitmap, 43, 44, 45, 46));
            ignoreException(() => ctx.drawImage(bitmap, 47, 48, 49, 410, 411, 412, 413, 414));
        },
        () => {
            ctx.drawImageFromRect?.(image, 1, 2, 3, 4, 5, 6, 7, 8)
            ctx.drawImageFromRect?.(image, 9, 10, 11, 12, 13, 14, 15, 16, "test");
        },
        () => {
            ignoreException(() => ctx.ellipse(1, 2, 3, 4, 5, 6, 7));
            ignoreException(() => ctx.ellipse(8, 9, 10, 11, 12, 13, 14, true));
        },
        () => {
            ctx.fill();
            ctx.fill("evenodd");
            ctx.fill(path12);
            ctx.fill(path34, "evenodd");
        },
        () => {
            ctx.fillRect(1, 2, 3, 4);
        },
        () => {
            ctx.fillStyle;
            ctx.fillStyle = "test";
            ctx.fillStyle = linearGradient;
            ctx.fillStyle = radialGradient;
            ctx.fillStyle = pattern;
        },
        () => {
            ctx.fillText("testA", 1, 2);
            ctx.fillText("testB", 3, 4, 5);
        },
        () => {
            ctx.font;
            ctx.font = "test";
        },
        () => {
            ignoreException(() => ctx.getImageData(1, 2, 3, 4));
        },
        () => {
            ctx.getLineDash();
        },
        () => {
            ctx.getTransform();
        },
        () => {
            ctx.globalAlpha;
            ctx.globalAlpha = 0;
        },
        () => {
            ctx.globalCompositeOperation;
            ctx.globalCompositeOperation = "test";
        },
        () => {
            ctx.imageSmoothingEnabled;
            ctx.imageSmoothingEnabled = true;
        },
        () => {
            ctx.imageSmoothingQuality;
            ctx.imageSmoothingQuality = "low";
        },
        () => {
            ctx.isPointInPath(path12, 5, 6);
            ctx.isPointInPath(path34, 7, 8, "evenodd");
            ctx.isPointInPath(9, 10);
            ctx.isPointInPath(11, 12, "evenodd");
        },
        () => {
            ctx.isPointInStroke(path12, 3, 4);
            ctx.isPointInStroke(5, 6);
        },
        () => {
            ctx.lineCap;
            ctx.lineCap = "test";
        },
        () => {
            ctx.lineDashOffset;
            ctx.lineDashOffset = 1;
        },
        () => {
            ctx.lineJoin;
            ctx.lineJoin = "test";
        },
        () => {
            ctx.lineTo(1, 2);
        },
        () => {
            ctx.lineWidth;
            ctx.lineWidth = 1;
        },
        () => {
            ctx.measureText("test");
        },
        () => {
            ctx.miterLimit;
            ctx.miterLimit = 1;
        },
        () => {
            ctx.moveTo(1, 2);
        },
        () => {
            ctx.putImageData(imageData14, 5, 6);
            ctx.putImageData(imageData23, 7, 8, 9, 10, 11, 12);
        },
        () => {
            ctx.quadraticCurveTo(1, 2, 3, 4);
        },
        () => {
            ctx.rect(1, 2, 3, 4);
        },
        () => {
            ctx.resetTransform();
        },
        () => {
            ctx.restore();
            --saveCount;
        },
        () => {
            ctx.rotate(1);
        },
        () => {
            ctx.save();
            ++saveCount;
        },
        () => {
            ctx.scale(1, 2);
        },
        () => {
            ctx.setAlpha?.();
            ctx.setAlpha?.(1);
        },
        () => {
            ctx.setCompositeOperation?.();
            ctx.setCompositeOperation?.("test");
        },
        () => {
            ctx.setFillColor?.("testA");
            ctx.setFillColor?.("testB", 1);
            ctx.setFillColor?.(2);
            ctx.setFillColor?.(3, 4);
            ctx.setFillColor?.(5, 6, 7, 8);
        },
        () => {
            ctx.setLineCap?.();
            ctx.setLineCap?.("test");
        },
        () => {
            ctx.setLineDash([1, 2]);
        },
        () => {
            ctx.setLineJoin?.();
            ctx.setLineJoin?.("test");
        },
        () => {
            ctx.setLineWidth?.();
            ctx.setLineWidth?.(1);
        },
        () => {
            ctx.setMiterLimit?.();
            ctx.setMiterLimit?.(1);
        },
        () => {
            ctx.setShadow?.(1, 2, 3);
            ctx.setShadow?.(4, 5, 6, "test", 7);
            ctx.setShadow?.(8, 9, 10, 11);
            ctx.setShadow?.(12, 13, 14, 15, 16);
            ctx.setShadow?.(17, 18, 19, 20, 21, 22, 23);
            ctx.setShadow?.(24, 25, 26, 27, 28, 29, 30, 31);
        },
        () => {
            ctx.setStrokeColor?.("testA");
            ctx.setStrokeColor?.("testB", 1);
            ctx.setStrokeColor?.(2);
            ctx.setStrokeColor?.(3, 4);
            ctx.setStrokeColor?.(5, 6, 7, 8);
        },
        () => {
            ctx.setTransform(1, 2, 3, 4, 5, 6);
            ignoreException(() => ctx.setTransform());
            ignoreException(() => ctx.setTransform(new DOMMatrix([7, 8, 9, 10, 11, 12])));
        },
        () => {
            ctx.shadowBlur;
            ctx.shadowBlur = 1;
        },
        () => {
            ctx.shadowColor;
            ctx.shadowColor = "test";
        },
        () => {
            ctx.shadowOffsetX;
            ctx.shadowOffsetX = 1;
        },
        () => {
            ctx.shadowOffsetY;
            ctx.shadowOffsetY = 1;
        },
        () => {
            ctx.stroke();
            ctx.stroke(path12);
        },
        () => {
            ctx.strokeRect(1, 2, 3, 4);
        },
        () => {
            ctx.strokeStyle;
            ctx.strokeStyle = "test";
            ctx.strokeStyle = linearGradient;
            ctx.strokeStyle = radialGradient;
            ctx.strokeStyle = pattern;
        },
        () => {
            ctx.strokeText("testA", 1, 2);
            ctx.strokeText("testB", 3, 4, 5);
        },
        () => {
            ctx.textAlign;
            ctx.textAlign = "test";
        },
        () => {
            ctx.textBaseline;
            ctx.textBaseline = "test";
        },
        () => {
            ctx.transform(1, 2, 3, 4, 5, 6);
        },
        () => {
            ctx.translate(1, 2);
        },
        () => {
            if (window.OffscreenCanvasRenderingContext2D && ctx instanceof OffscreenCanvasRenderingContext2D)
                return;
            ctx.webkitImageSmoothingEnabled;
            ctx.webkitImageSmoothingEnabled = true;
        },
        () => {
            if (window.OffscreenCanvasRenderingContext2D && ctx instanceof OffscreenCanvasRenderingContext2D)
                return;
            ctx.webkitLineDash;
            ctx.webkitLineDash = [1, 2];
        },
        () => {
            if (window.OffscreenCanvasRenderingContext2D && ctx instanceof OffscreenCanvasRenderingContext2D)
                return;
            ctx.webkitLineDashOffset;
            ctx.webkitLineDashOffset = 1;
        },
        () => {
            ctx.canvas.width;
            ctx.canvas.width = 2;
        },
        () => {
            ctx.canvas.height;
            ctx.canvas.height = 2;
        },
        () => {
            // FIXME Add overload testing the non-vector DOMPointInit after bug233255
            ctx.roundRect(0, 0, 50, 50, 42);
            ctx.roundRect(0, 0, 50, 50, [23]);
            ctx.roundRect(0, 0, 150, 150, [{x: 24, y: 42}]);
        },
        /* FIXME: Disabled as per webkit.org/b/272591. Should be fully removed if we manage to remove this method.
        () => {
            ctx.commit?.();
        },
        */
        () => {
            TestPage.dispatchEventToFrontend("LastFrame");
        },
    ];
    let index = 0;
    function executeFrameFunction() {
        frames[index++]();
        if (index < frames.length)
            requestAnimationFrameId = requestAnimationFrame(executeFrameFunction);
    };
    executeFrameFunction();
}

function performConsoleActions() {
    console.record(ctx, {name: "TEST"});

    ctx.fill();

    console.recordEnd(ctx);

    ctx.stroke();
}

function performSavePreActions() {
    cancelActions();
    ctx.restore();
    ctx.restore();

    function saveAndSet(translateX, translateY, globalAlpha, globalCompositeOperation, lineWidth, lineCap, lineJoin, miterLimit, shadowOffsetX, shadowOffsetY, shadowBlur, shadowColor, lineDash, lineDashOffset, font, textAlign, textBaseline, direction, strokeStyle, fillStyle, imageSmoothingEnabled, imageSmoothingQuality) {
        ctx.save()

        ctx.translate(translateX, translateY)
        ctx.globalAlpha = globalAlpha
        ctx.globalCompositeOperation = globalCompositeOperation
        ctx.lineWidth = lineWidth
        ctx.lineCap = lineCap
        ctx.lineJoin = lineJoin
        ctx.miterLimit = miterLimit
        ctx.shadowOffsetX = shadowOffsetX
        ctx.shadowOffsetY = shadowOffsetY
        ctx.shadowBlur = shadowBlur
        ctx.shadowColor = shadowColor
        ctx.setLineDash(lineDash)
        ctx.lineDashOffset = lineDashOffset
        ctx.font = font
        ctx.textAlign = textAlign
        ctx.textBaseline = textBaseline
        ctx.direction = direction
        ctx.strokeStyle = strokeStyle
        ctx.fillStyle = fillStyle
        ctx.imageSmoothingEnabled = imageSmoothingEnabled
        ctx.imageSmoothingQuality = imageSmoothingQuality
        // FIXME: Change the path, too.
    }

    saveAndSet(1, 0, 0.5, "source-in", 0.5, "round", "bevel", 20, 2, 3, 4, "#100000", [1, 2], 10, "20px sans-serif", "left", "top", "ltr", pattern, linearGradient, false, "medium")
    saveAndSet(0, 1, 0, "difference", 2, "square", "round", 30, 4, 5, 6, "#001000", [3, 4], 11, "30px cursive", "right", "hanging", "inherit", linearGradient, pattern, true, "high")
    saveAndSet(-1, -2, 0.75, "source-over", 3, "round", "bevel", 40, 6, 7, 8, "#000010", [5, 6], 12, "40px fantasy", "center", "ideographic", "rtl", "#200000", "#300000", false, "medium")
}

function performSavePostActions() {
    ctx.fill();
}
