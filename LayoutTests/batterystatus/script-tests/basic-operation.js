description('Tests the basic operation of BatteryStatus.');

var charging = false;
var chargingTime = Number.POSITIVE_INFINITY;
var dischargingTime = 6000;
var level = 0.7;

var battery = navigator.webkitBattery;

battery.addEventListener('chargingchange', function() {
    shouldBe('battery.charging', 'charging');
    shouldBe('battery.chargingTime', 'chargingTime');
    shouldBe('battery.dischargingTime', 'dischargingTime');
    shouldBe('battery.level', 'level');
    finishJSTest();
});

internals.setBatteryStatus(document, 'chargingchange', charging, chargingTime, dischargingTime, level);
window.jsTestIsAsync = true;
