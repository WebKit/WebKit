
var MockRemoteAPI = {
    url: function (path)
    {
        return `${this.urlPrefix}${path}`;
    },
    getJSON: function (url)
    {
        return this.getJSONWithStatus(url);
    },
    getJSONWithStatus: function (url)
    {
        return this._addRequest(url, 'GET', null);
    },
    postJSON: function (url, data)
    {
        return this._addRequest(url, 'POST', data);
    },
    postJSONWithStatus: function (url, data)
    {
        return this._addRequest(url, 'POST', data);
    },
    postFormUrlencodedData: function (url, data)
    {
        return this._addRequest(url, 'POST', data);
    },
    _addRequest: function (url, method, data)
    {
        var request = {
            url: url,
            method: method,
            data: data,
            promise: null,
            resolve: null,
            reject: null,
        };

        request.promise = new Promise(function (resolve, reject) {
            request.resolve = resolve;
            request.reject = reject;
        });

        if (this._waitingPromise) {
            this._waitingPromiseResolver();
            this._waitingPromise = null;
            this._waitingPromiseResolver = null;
        }

        MockRemoteAPI.requests.push(request);
        return request.promise;
    },
    waitForRequest()
    {
        if (!this._waitingPromise) {
            this._waitingPromise = new Promise(function (resolve, reject) {
                MockRemoteAPI._waitingPromiseResolver = resolve;
            });
        }
        return this._waitingPromise;
    },
    inject: function (urlPrefix, privilegedAPI)
    {
        let originalRemoteAPI;
        let originalPrivilegedAPI;

        beforeEach(() => {
            MockRemoteAPI.reset(urlPrefix);
            originalRemoteAPI = global.RemoteAPI;
            global.RemoteAPI = MockRemoteAPI;
            originalPrivilegedAPI = global.PrivilegedAPI;
            global.PrivilegedAPI = privilegedAPI;
            if (privilegedAPI && privilegedAPI._token)
                privilegedAPI._token = null;
        });

        afterEach(() => {
            global.RemoteAPI = originalRemoteAPI;
            global.PrivilegedAPI = originalPrivilegedAPI;
            if (privilegedAPI && privilegedAPI._token)
                privilegedAPI._token = null;
        });

        return MockRemoteAPI.requests;
    },
    reset: function (urlPrefix)
    {
        if (urlPrefix)
            MockRemoteAPI.urlPrefix = urlPrefix;
        MockRemoteAPI.requests.length = 0;
    },
    requests: [],
    _waitingPromise: null,
    _waitingPromiseResolver: null,
    urlPrefix: 'http://mockhost',
};

if (typeof module != 'undefined')
    module.exports.MockRemoteAPI = MockRemoteAPI;
