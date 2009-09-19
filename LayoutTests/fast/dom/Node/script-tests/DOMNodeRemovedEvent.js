description("This test checks that DOMNodeRemovedEvent is emitted once (and only once).");

var div = document.createElement("div");
document.body.appendChild(div);

var count = 0;
document.body.addEventListener("DOMNodeRemoved", function () { count++; }, false);
document.body.removeChild(div);

shouldBe("count", "1");

var successfullyParsed = true;
