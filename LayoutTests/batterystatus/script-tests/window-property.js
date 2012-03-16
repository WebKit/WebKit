description('Tests that the window.navigator.battery properties are present.');

function hasOnBatteryProperty()
{
    var result = 0;
    for (var property in navigator.webkitBattery) {
        if (property == 'onchargingchange' ||
            property == 'onchargingtimechange' ||
            property == 'ondischargingtimechange' ||
            property == 'onlevelchange')
            result += 1;
    }
    if (result == 4)
        return true;
    return false;
}

shouldBeTrue("typeof navigator.webkitBattery == 'object'");
shouldBeTrue("hasOnBatteryProperty()");
shouldBeTrue("navigator.webkitBattery.hasOwnProperty('onchargingchange')");
shouldBeTrue("navigator.webkitBattery.hasOwnProperty('onchargingtimechange')");
shouldBeTrue("navigator.webkitBattery.hasOwnProperty('ondischargingtimechange')");
shouldBeTrue("navigator.webkitBattery.hasOwnProperty('onlevelchange')");
