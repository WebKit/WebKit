
class Repository extends LabeledObject {
    constructor(id, object)
    {
        super(id, object);
        this._url = object.url;
        this._blameUrl = object.blameUrl;
        this._hasReportedCommits = object.hasReportedCommits;
    }

    hasUrlForRevision() { return !!this._url; }

    urlForRevision(currentRevision)
    {
        return (this._url || '').replace(/\$1/g, currentRevision);
    }

    urlForRevisionRange(from, to)
    {
        return (this._blameUrl || '').replace(/\$1/g, from).replace(/\$2/g, to);
    }
}
