
class Bug extends DataModelObject {
    constructor(id, object)
    {
        super(id, object);

        console.assert(object.bugTracker instanceof BugTracker);
        this._bugTracker = object.bugTracker;
        this._bugNumber = object.number;
    }

    static ensureSingleton(object)
    {
        console.assert(object.bugTracker instanceof BugTracker);
        var id = object.bugTracker.id() + '-' + object.number;
        return super.ensureSingleton(id, object);
    }

    updateSingleton(object)
    {
        super.updateSingleton(object);
        console.assert(this._bugTracker == object.bugTracker);
        console.assert(this._bugNumber == object.number);
    }

    bugTracker() { return this._bugTracker; }
    bugNumber() { return this._bugNumber; }
    url() { return this._bugTracker.bugUrl(this._bugNumber); }
    label() { return this.bugNumber(); }
    title() { return `${this._bugTracker.label()}: ${this.bugNumber()}`; }
}

if (typeof module != 'undefined')
    module.exports.Bug = Bug;