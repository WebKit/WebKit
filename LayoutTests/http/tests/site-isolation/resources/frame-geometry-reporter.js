(function () {
    var label = new URLSearchParams(location.search).get("label");

    function toArray(rect)
    {
        return [rect.x, rect.y, rect.width, rect.height];
    }

    function report()
    {
        if (!window.internals) {
            top.postMessage({ label: label }, "*");
            return;
        }

        top.postMessage({
            label: label,
            windowClipRect: toArray(internals.windowClipRect()),
            exposedContentRect: toArray(internals.exposedContentRect())
        }, "*");
    }

    window.addEventListener("message", function (event) {
        if (event.data !== "report-frame-geometry")
            return;

        report();

        // Forward the request to this frame's children.
        for (var i = 0; i < window.length; i++)
            window[i].postMessage(event.data, "*");
    }, false);
})();
