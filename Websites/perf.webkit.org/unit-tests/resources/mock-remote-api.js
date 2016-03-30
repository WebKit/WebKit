var assert = require('assert');

if (!assert.notReached)
    assert.notReached = function () { assert(false, 'This code path should not be reached'); }

global.requests = [];
global.RemoteAPI = {
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

        requests.push(request);
        return request.promise;
    },
};

beforeEach(function () {
    requests = [];
});
