var targetDiv = document.createElement("div");
targetDiv.id = "targetDiv";
targetDiv.setAttribute('style', "position: absolute; top: 100px; left: 10px; width: 400px; height: 400px; overflow: scroll;");

var innerDiv = document.createElement("div");
innerDiv.setAttribute('style', "width: 800px; height: 800px;");

targetDiv.appendChild(innerDiv);

document.body.insertBefore(targetDiv, document.getElementById('console'));

description("Tests that touch events cause an overflow:scroll div to scroll.");

function verifyScrollOffset(offsetX, offsetY)
{
    shouldBe("targetDiv.scrollLeft", offsetX.toString());
    shouldBe("targetDiv.scrollTop", offsetY.toString());
}

if (window.eventSender) {
    verifyScrollOffset(0, 0);

    // Test vertical up scroll.
    eventSender.clearTouchPoints();
    eventSender.addTouchPoint(20, 220);
    eventSender.touchStart();
    eventSender.updateTouchPoint(0, 20, 120);
    eventSender.touchMove();
    eventSender.touchEnd();
    verifyScrollOffset(0, 100);

    // Test vertical down scroll.
    eventSender.clearTouchPoints();
    eventSender.addTouchPoint(20, 120);
    eventSender.touchStart();
    eventSender.updateTouchPoint(0, 20, 170);
    eventSender.touchMove();
    eventSender.touchEnd();
    verifyScrollOffset(0, 50);

    // Test diagonal scroll
    eventSender.clearTouchPoints();
    eventSender.addTouchPoint(120, 120);
    eventSender.touchStart();
    eventSender.updateTouchPoint(0, 20, 20);
    eventSender.touchMove();
    eventSender.touchEnd();

    verifyScrollOffset(100, 150);

} else
    debug('This test requires DRT.');

var successfullyParsed = true;
