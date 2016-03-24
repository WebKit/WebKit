'use strict';

class BuildRequest extends DataModelObject {

    constructor(id, object)
    {
        super(id, object);
        console.assert(!object.testGroup || object.testGroup instanceof TestGroup);
        this._testGroup = object.testGroup;
        if (this._testGroup)
            this._testGroup.addBuildRequest(this);
        this._order = object.order;
        console.assert(object.rootSet instanceof RootSet);
        this._rootSet = object.rootSet;
        this._status = object.status;
        this._statusUrl = object.url;
        this._buildId = object.build;
        this._result = null;
    }

    updateSingleton(object)
    {
        console.assert(this._testGroup == object.testGroup);
        console.assert(this._order == object.order);
        console.assert(this._rootSet == object.rootSet);
        this._status = object.status;
        this._statusUrl = object.url;
        this._buildId = object.build;
    }

    testGroup() { return this._testGroup; }
    order() { return this._order; }
    rootSet() { return this._rootSet; }

    hasFinished() { return this._status == 'failed' || this._status == 'completed' || this._status == 'canceled'; }
    hasStarted() { return this._status != 'pending'; }
    hasPending() { return this._status == 'pending'; }
    statusLabel()
    {
        switch (this._status) {
        case 'pending':
            return 'Waiting to be scheduled';
        case 'scheduled':
            return 'Scheduled';
        case 'running':
            return 'Running';
        case 'failed':
            return 'Failed';
        case 'completed':
            return 'Completed';
        case 'canceled':
            return 'Canceled';
        }
    }
    statusUrl() { return this._statusUrl; }

    buildId() { return this._buildId; }

    result() { return this._result; }
    setResult(result)
    {
        this._result = result;
        this._testGroup.didSetResult(this);
    }
}

if (typeof module != 'undefined')
    module.exports.BuildRequest = BuildRequest;
