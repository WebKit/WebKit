var focusinSeen = "";
var focusoutSeen = "";

var rectElement = document.getElementById("rect1");
var gElement = document.getElementById("g");
var useElement = document.getElementById("use");
var useElement2 = document.getElementById("usesymbol");
var switchElement = document.getElementById("switch");
var imgElement = document.getElementById("img");
description("Test whether focusin and focusout events are dispatched and seen in the focusin/focusout event handlers when using keyboard: ");

if (window.testRunner)
    testRunner.waitUntilDone();

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

if (window.eventSender) {

    // cause focusin and focusout
    eventSender.keyDown('\t');
    shouldBeEqualToString('focusinSeen', 'rect1');
    eventSender.keyDown('\t');
    shouldBeEqualToString('focusinSeen', 'g');
    shouldBeEqualToString('focusoutSeen', 'rect1');
    eventSender.keyDown('\t');
    shouldBeEqualToString('focusinSeen', 'use');
    shouldBeEqualToString('focusoutSeen', 'g');
    eventSender.keyDown('\t');
    shouldBeEqualToString('focusinSeen', 'usesymbol');
    shouldBeEqualToString('focusoutSeen', 'use');
    eventSender.keyDown('\t');
    shouldBeEqualToString('focusinSeen', 'switch');
    shouldBeEqualToString('focusoutSeen', 'usesymbol');
    eventSender.keyDown('\t');
    shouldBeEqualToString('focusinSeen', 'img');
    shouldBeEqualToString('focusoutSeen', 'switch');
    eventSender.keyDown('\t');
    shouldBeEqualToString('focusoutSeen', 'img');

    successfullyParsed = true;

    if (window.testRunner)
        testRunner.notifyDone();
} else
    alert("This test must be run via DRT!");
