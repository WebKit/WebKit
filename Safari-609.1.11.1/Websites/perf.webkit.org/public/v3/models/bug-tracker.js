'use strict';

class BugTracker extends LabeledObject {
    constructor(id, object)
    {
        super(id, object);
        this._bugUrl = object.bugUrl;
        this._newBugUrl = object.newBugUrl;
        this._repositories = object.repositories;
    }

    bugUrl(bugNumber) { return this._bugUrl && bugNumber ? this._bugUrl.replace(/\$number/g, bugNumber) : null; }
    newBugUrl() { return this._newBugUrl; }
    repositories() { return this._repositories; }
}

if (typeof module != 'undefined')
    module.exports.BugTracker = BugTracker;
