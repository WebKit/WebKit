description('Tests that adding a new event listener from a callback works as expected.');

var charging = false;
var chargingTime = Number.POSITIVE_INFINITY;
var dischargingTime = 6000;
var level = 0.7;

var battery = navigator.webkitBattery;

function checkBatteryStatus() {
    shouldBe("battery.charging", "false");
    shouldBe("battery.chargingTime", "Infinity");
    shouldBe("battery.dischargingTime", "6000");
    shouldBe("battery.level", "0.7");
}

var firstListenerEvents = 0;
function firstListener() {
    checkBatteryStatus();
    if (++firstListenerEvents == 1) {
        battery.addEventListener('chargingchange', secondListener);
        internals.setBatteryStatus(document, 'chargingchange', charging, chargingTime, dischargingTime, level);
    }
    else if (firstListenerEvents > 2)
        testFailed('Too many events for first listener.');
    maybeFinishTest();
}

var secondListenerEvents = 0;
function secondListener() {
    checkBatteryStatus();
    if (++secondListenerEvents > 1)
        testFailed('Too many events for second listener.');
    maybeFinishTest();
}

function maybeFinishTest() {
    if (firstListenerEvents == 2 && secondListenerEvents == 1)
        finishJSTest();
}

battery.addEventListener('chargingchange', firstListener);
internals.setBatteryStatus(document, 'chargingchange', charging, chargingTime, dischargingTime, level);
window.jsTestIsAsync = true;
