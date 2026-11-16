testRunner?.waitUntilDone();

function addMarker(x, y, color)
{
    const marker = document.createElement("div");
    marker.style.cssText = `position: fixed; left: ${x}px; top: ${y}px; width: 10px; height: 10px; background-color: ${color};`;
    document.body.appendChild(marker);
}

addEventListener("message", async (event) => {
    if (event.data == "scrolled") {
        await eventSender.asyncMouseMoveTo(300, 240);
        await eventSender.asyncMouseDown();
        await eventSender.asyncMouseUp();
        return;
    }

    addMarker(event.data.clientX, event.data.clientY, "green");
    addMarker(event.data.pageX, event.data.pageY, "blue");

    testRunner?.notifyDone();
});

function clickInScrolledSubframe(subframeOrigin)
{
    document.body.style.margin = "0";

    const subframe = document.createElement("iframe");
    subframe.style.cssText = "position: absolute; left: 50px; top: 40px; width: 600px; height: 500px; border: none;";
    subframe.src = `${subframeOrigin}/site-isolation/mouse-events/resources/scroll-and-message-mouse-down-client-coordinates.html`;
    document.body.appendChild(subframe);
}
