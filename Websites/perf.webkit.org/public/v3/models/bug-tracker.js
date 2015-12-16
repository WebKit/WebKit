
class BugTracker extends LabeledObject {
    constructor(id, object)
    {
        super(id, object);
        this._bugUrl = object.bugUrl;
        this._newBugUrl = object.newBugUrl;
        this._repositories = object.repositories;
    }

    bugUrl(bugNumber) { return this._bugUrl && bugNumber ? this._bugUrl.replace(/\$number/g, bugNumber) : null; }
}
