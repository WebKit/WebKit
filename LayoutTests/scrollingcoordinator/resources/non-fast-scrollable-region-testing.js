function runNonFastScrollableRegionTest(scale) {
    var invScale;
    window.internals.settings.setMockScrollbarsEnabled(true);
    if (scale != undefined) {
        window.internals.setPageScaleFactor(scale, 0, 0);
        // FIXME: This is a hack for applyPageScaleFactorInCompositor() == false.
        invScale = 1 / scale;
    } else {
        invScale = 1;
    }

    var overlay = document.createElement("div");
    overlay.style.position = "absolute";
    overlay.style.left = 0;
    overlay.style.top = 0;
    overlay.style.opacity = 0.5;

    var rects = window.internals.nonFastScrollableRects(document);
    for (var i = 0; i < rects.length; i++) {
        var rect = rects[i];
        var patch = document.createElement("div");
        patch.style.position = "absolute";
        patch.style.backgroundColor = "#00ff00";
        patch.style.left = rect.left * invScale + "px";
        patch.style.top = rect.top * invScale + "px";
        patch.style.width = rect.width * invScale + "px";
        patch.style.height = rect.height * invScale + "px";

        overlay.appendChild(patch);
    }

    document.body.appendChild(overlay);
}

