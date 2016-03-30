var assert = require('assert');

if (!assert.notReached)
    assert.notReached = function () { assert(false, 'This code path should not be reached'); }

var MockRemoteAPI = {
    getJSON: function (url)
    {
        return this.getJSONWithStatus(url);
    },
    getJSONWithStatus: function (url)
    {
        var request = {
            url: url,
            promise: null,
            resolve: null,
            reject: null,
        };

        request.promise = new Promise(function (resolve, reject) {
            request.resolve = resolve;
            request.reject = reject;
        });

        MockRemoteAPI.requests.push(request);
        return request.promise;
    },
    inject: function ()
    {
        var originalRemoteAPI = global.RemoteAPI;

        beforeEach(function () {
            MockRemoteAPI.requests.length = 0;
            originalRemoteAPI = global.RemoteAPI;
            global.RemoteAPI = MockRemoteAPI;
        });

        afterEach(function () {        
            global.RemoteAPI = originalRemoteAPI;
        });

        return MockRemoteAPI.requests;
    }
};
MockRemoteAPI.requests = [];

if (typeof module != 'undefined')
    module.exports.MockRemoteAPI = MockRemoteAPI;
