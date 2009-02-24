description("Ensure that clicking in the margins around editable divs does not grant focus.")

document.body.style.margin = "20px";

var outerDiv = document.createElement("div");
outerDiv.style.cssText = "width: 100px; padding: 100px; background-color: blue;";
var innerDiv = document.createElement("div");
innerDiv.style.cssText = "height: 100px; border: 1px solid pink; background-color: green;";
innerDiv.contentEditable = true;
outerDiv.appendChild(innerDiv);
document.body.insertBefore(outerDiv, document.body.firstChild);

var lastClickCausedFocus = false;
document.documentElement.addEventListener("mouseup", function(e) {
    var lastClick = e || window.event;
    if (lastClickCausedFocus) {
        testFailed("Click @ " + lastClick.clientX + ", " + lastClick.clientY + " caused innerdiv to focus!");
    } else {
        testPassed("Click @ " + lastClick.clientX + ", " + lastClick.clientY + " did not cause focus.");
    }
    innerDiv.blur();
    lastClickCausedFocus = false;
}, false);

innerDiv.addEventListener("focus", function() {
    lastClickCausedFocus = true;
}, false);

if (window.eventSender) {
    clickAt(10, 10);
    clickAt(170, 10);
    clickAt(10, 170);

    clickAt(70, 70);
    clickAt(170, 70);
    clickAt(270, 70);

    clickAt(70, 170);
    clickAt(270, 170);

    clickAt(70, 270);
    clickAt(170, 270);
    clickAt(270, 270);
} else {
    debug("To test, cick above and to the left of the blue box, then above it, then to the left " +
          "of it and finally, click in all 8 regions around the green div.");
}

var successfullyParsed = true;
