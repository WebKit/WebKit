
class AsyncTask {

    constructor(method, args)
    {
        this._method = method;
        this._args = args;
    }

    execute()
    {
        if (!(this._method in Statistics))
            throw `${this._method} is not a valid method of Statistics`;

        AsyncTask._asyncMessageId++;

        var startTime = Date.now();
        var method = this._method;
        var args = this._args;
        return new Promise(function (resolve, reject) {
            AsyncTaskWorker.waitForAvailableWorker(function (worker) {
                worker.sendTask({id: AsyncTask._asyncMessageId, method: method, args: args}).then(function (data) {
                    var startLatency = data.workerStartTime - startTime;
                    var totalTime = Date.now() - startTime;
                    var callback = data.status == 'resolve' ? resolve : reject;
                    callback({result: data.result, workerId: worker.id(), startLatency: startLatency, totalTime: totalTime, workerTime: data.workerTime});
                });
            });
        });
    }

    static isAvailable()
    {
        return typeof Worker !== 'undefined';
    }
}

AsyncTask._asyncMessageId = 0;

class AsyncTaskWorker {

    // Takes a callback instead of returning a promise because a worker can become unavailable before the end of the current microtask.
    static waitForAvailableWorker(callback)
    {
        var worker = this._makeWorkerEventuallyAvailable();
        if (worker)
            callback(worker);
        this._queue.push(callback);
    }

    static _makeWorkerEventuallyAvailable()
    {
        var worker = this._findAvailableWorker();
        if (worker)
            return worker;

        var canStartMoreWorker = this._workerSet.size < this._maxWorkerCount;
        if (!canStartMoreWorker)
            return null;

        if (this._latestStartTime > Date.now() - 50) {
            setTimeout(function () {
                var worker = AsyncTaskWorker._findAvailableWorker();
                if (worker)
                    AsyncTaskWorker._queue.pop()(worker);
            }, 50);
            return null;
        }
        return new AsyncTaskWorker;
    }

    static _findAvailableWorker()
    {
        for (var worker of this._workerSet) {
            if (!worker._currentTaskId)
                return worker;
        }
        return null;
    }

    constructor()
    {
        this._webWorker = new Worker('async-task.js');
        this._webWorker.onmessage = this._didRecieveMessage.bind(this);
        this._id = AsyncTaskWorker._workerId;
        this._startTime = Date.now();
        this._currentTaskId = null;
        this._callback = null;

        AsyncTaskWorker._latestStartTime = this._startTime;
        AsyncTaskWorker._workerId++;
        AsyncTaskWorker._workerSet.add(this);
    }

    id() { return this._id; }

    sendTask(task)
    {
        console.assert(!this._currentTaskId);
        console.assert(task.id);
        var self = this;
        this._currentTaskId = task.id;
        return new Promise(function (resolve) {
            self._webWorker.postMessage(task);
            self._callback = resolve;
        });
    }

    _didRecieveMessage(event)
    {
        var callback = this._callback;

        console.assert(this._currentTaskId);
        this._currentTaskId = null;
        this._callback = null;

        if (AsyncTaskWorker._queue.length)
            AsyncTaskWorker._queue.pop()(this);
        else {
            var self = this;
            setTimeout(function () {
                if (self._currentTaskId == null)
                    AsyncTaskWorker._workerSet.delete(self);
            }, 1000);
        }

        callback(event.data);
    }

    static workerDidRecieveMessage(event)
    {
        var data = event.data;
        var id = data.id;
        var method = Statistics[data.method];
        var startTime = Date.now();
        try {
            var returnValue = method.apply(Statistics, data.args);
            postMessage({'id': id, 'status': 'resolve', 'result': returnValue, 'workerStartTime': startTime, 'workerTime': Date.now() - startTime});
        } catch (error) {
            postMessage({'id': id, 'status': 'reject', 'result': error.toString(), 'workerStartTime': startTime, 'workerTime': Date.now() - startTime});
            throw error;
        }
    }
}

AsyncTaskWorker._maxWorkerCount = typeof navigator != 'undefined' && 'hardwareConcurrency' in navigator ? Math.max(1, navigator.hardwareConcurrency - 1) : 1;
AsyncTaskWorker._workerSet = new Set;
AsyncTaskWorker._queue = [];
AsyncTaskWorker._workerId = 1;
AsyncTaskWorker._latestStartTime = 0;

if (typeof module == 'undefined' && typeof window == 'undefined' && typeof importScripts != 'undefined') { // Inside a worker
    importScripts('/shared/statistics.js');
    onmessage = AsyncTaskWorker.workerDidRecieveMessage.bind(AsyncTaskWorker);
}

if (typeof module != 'undefined')
    module.exports.AsyncTask = AsyncTask;