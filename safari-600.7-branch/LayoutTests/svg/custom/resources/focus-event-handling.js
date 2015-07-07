var focusinSeen = "";
var focusoutSeen = "";

var rectElement = document.getElementById("rect1");
var gElement = document.getElementById("g");
var useElement = document.getElementById("use");
var useElement2 = document.getElementById("usesymbol");
var switchElement = document.getElementById("switch");
var imgElement = document.getElementById("img");
description("Test whether focusin and focusout events are dispatched and seen in the focusin/focusout event handlers: ");

if (window.testRunner)
    testRunner.waitUntilDone();

function clearFocusSeen(evt)
{
    focusinSeen = "";
    focusoutSeen = "";
}

function focusinHandler(evt)
{
    focusinSeen = evt.target.getAttribute('id');
}

function focusoutHandler(evt)
{
    focusoutSeen = evt.target.getAttribute('id');
}

rectElement.setAttribute("onfocusin", "focusinHandler(evt)");
rectElement.setAttribute("onfocusout", "focusoutHandler(evt)");
gElement.setAttribute("onfocusin", "focusinHandler(evt)");
gElement.setAttribute("onfocusout", "focusoutHandler(evt)");
useElement.setAttribute("onfocusin", "focusinHandler(evt)");
useElement.setAttribute("onfocusout", "focusoutHandler(evt)");
useElement2.setAttribute("onfocusin", "focusinHandler(evt)");
useElement2.setAttribute("onfocusout", "focusoutHandler(evt)");
switchElement.setAttribute("onfocusin", "focusinHandler(evt)");
switchElement.setAttribute("onfocusout", "focusoutHandler(evt)");
imgElement.setAttribute("onfocusin", "focusinHandler(evt)");
imgElement.setAttribute("onfocusout", "focusoutHandler(evt)");

function clickAt(x, y)
{
    eventSender.mouseMoveTo(x, y);
    eventSender.mouseDown();
    eventSender.mouseUp();
}

if (window.eventSender) {

    // cause focusin and focusout
    clickAt(50, 50);
    clickAt(150, 250);
    shouldBeEqualToString('focusinSeen', 'rect1');
    shouldBeEqualToString('focusoutSeen', 'rect1');

    clearFocusSeen();

    // cause focusin and focusout
    clickAt(150, 50);
    clickAt(150, 250);
    shouldBeEqualToString('focusinSeen', 'g');
    shouldBeEqualToString('focusoutSeen', 'g');

    clearFocusSeen();

    // cause focusin and focusout
    clickAt(250, 50);
    clickAt(250, 250);
    shouldBeEqualToString('focusinSeen', 'use');
    shouldBeEqualToString('focusoutSeen', 'use');

    clearFocusSeen();

    // cause focusin and focusout
    clickAt(350, 50);
    clickAt(350, 250);
    shouldBeEqualToString('focusinSeen', 'usesymbol');
    shouldBeEqualToString('focusoutSeen', 'usesymbol');

    clearFocusSeen();

    // cause focusin and focusout
    clickAt(50, 150);
    clickAt(50, 250);
    shouldBeEqualToString('focusinSeen', 'switch');
    shouldBeEqualToString('focusoutSeen', 'switch');

    clearFocusSeen();

    // cause focusin and focusout
    clickAt(150, 150);
    clickAt(150, 250);
    shouldBeEqualToString('focusinSeen', 'img');
    shouldBeEqualToString('focusoutSeen', 'img');

    successfullyParsed = true;
    successfullyParsed = true;

    if (window.testRunner)
        testRunner.notifyDone();
} else
    alert("This test must be run via DRT!");
