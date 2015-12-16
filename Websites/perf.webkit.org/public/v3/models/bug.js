
class Bug extends DataModelObject {
    constructor(id, object)
    {
        super(id, object);

        console.assert(object.bugTracker instanceof BugTracker);
        this._bugTracker = object.bugTracker;
        this._bugNumber = object.number;
    }

    bugTracker() { return this._bugTracker; }
    bugNumber() { return this._bugNumber; }
    url() { return this._bugTracker.bugUrl(this._bugNumber); }
    label() { return this.bugNumber(); }
    title() { return `${this._bugTracker.label()}: ${this.bugNumber()}`; }
}
