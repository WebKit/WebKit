description('Tests the basic operation of all BatteryStatus events.');

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

battery.addEventListener('chargingchange', function() {
    debug('chargingchange event is raised');
    checkBatteryStatus();
});

battery.addEventListener('chargingtimechange', function() {
    debug('chargingtimechange event is raised');
    checkBatteryStatus();
});

battery.addEventListener('dischargingtimechange', function() {
    debug('dischargingtimechange event is raised');
    checkBatteryStatus();
});

battery.addEventListener('levelchange', function() {
    debug('levelchange event is raised');
    checkBatteryStatus();
    finishJSTest();
});

internals.setBatteryStatus('chargingchange', charging, chargingTime, dischargingTime, level);
internals.setBatteryStatus('chargingtimechange', charging, chargingTime, dischargingTime, level);
internals.setBatteryStatus('dischargingtimechange', charging, chargingTime, dischargingTime, level);
internals.setBatteryStatus('levelchange', charging, chargingTime, dischargingTime, level);
window.jsTestIsAsync = true;
