
// Export NetworkSimulator for use by other unittests.
function NetworkSimulator()
{
    this._pendingCallbacks = [];
};

NetworkSimulator.prototype.scheduleCallback = function(callback)
{
    this._pendingCallbacks.push(callback);
};

NetworkSimulator.prototype.runTest = function(testCase)
{
    var self = this;
    var realNet = window.net;

    window.net = {};
    if (self.probe)
        net.probe = self.probe;
    if (self.jsonp)
        net.jsonp = self.jsonp;

    testCase();

    while (this._pendingCallbacks.length) {
        var callback = this._pendingCallbacks.shift();
        callback();
    }

    window.net = realNet;
    equal(window.net, realNet, "Failed to restore real base!");
};

(function () {

module("net");

// No unit tests yet!

})();
