
class BuildRequest extends DataModelObject {

    constructor(id, object)
    {
        super(id, object);
        console.assert(object.testGroup instanceof TestGroup);
        this._testGroup = object.testGroup;
        this._testGroup.addBuildRequest(this);
        this._order = object.order;
        console.assert(object.rootSet instanceof RootSet);
        this._rootSet = object.rootSet;
        this._status = object.status;
        this._statusUrl = object.url;
        this._buildId = object.build;
        this._result = null;
    }

    testGroup() { return this._testGroup; }
    order() { return this._order; }
    rootSet() { return this._rootSet; }

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
