'use strict';

class CommitLog extends DataModelObject {
    constructor(id, rawData)
    {
        console.assert(parseInt(id) == id);
        super(id);
        this._repository = rawData.repository;
        console.assert(this._repository instanceof Repository);
        this._rawData = rawData;
        this._remoteId = rawData.id;
        if (this._remoteId)
            this.ensureNamedStaticMap('remoteId')[this._remoteId] = this;
    }

    updateSingleton(rawData)
    {
        super.updateSingleton(rawData);

        console.assert(+this._rawData['time'] == +rawData['time']);
        console.assert(this._rawData['revision'] == rawData['revision']);

        if (rawData.authorName)
            this._rawData.authorName = rawData.authorName;
        if (rawData.message)
            this._rawData.message = rawData.message;
    }

    repository() { return this._repository; }
    time() { return new Date(this._rawData['time']); }
    author() { return this._rawData['authorName']; }
    revision() { return this._rawData['revision']; }
    message() { return this._rawData['message']; }
    url() { return this._repository.urlForRevision(this._rawData['revision']); }

    label()
    {
        var revision = this.revision();
        if (parseInt(revision) == revision) // e.g. r12345
            return 'r' + revision;
        else if (revision.length == 40) // e.g. git hash
            return revision.substring(0, 8);
        return revision;
    }
    title() { return this._repository.name() + ' at ' + this.label(); }

    diff(previousCommit)
    {
        if (this == previousCommit)
            previousCommit = null;

        const repository = this._repository;
        if (!previousCommit)
            return {repository: repository, label: this.label(), url: this.url()};

        const to = this.revision();
        const from = previousCommit.revision();
        let fromRevisionForURL = from;
        let label = null;
        if (parseInt(from) == from) { // e.g. r12345.
            fromRevisionForURL = (parseInt(from) + 1).toString;
            label = `r${from}-r${this.revision()}`;
        } else if (to.length == 40) // e.g. git hash
            label = `${from.substring(0, 8)}..${to.substring(0, 8)}`;
        else
            label = `${from} - ${to}`;

        return {repository: repository, label: label, url: repository.urlForRevisionRange(from, to)};
    }

    static fetchBetweenRevisions(repository, precedingRevision, lastRevision)
    {
        // FIXME: The cache should be smarter about fetching a range within an already fetched range, etc...
        // FIXME: We should evict some entires from the cache in cachedFetch.
        return this.cachedFetch(`/api/commits/${repository.id()}/`, {precedingRevision, lastRevision})
            .then((data) => this._constructFromRawData(repository, data));
    }

    static fetchForSingleRevision(repository, revision)
    {
        return this.cachedFetch(`/api/commits/${repository.id()}/${revision}`).then((data) => {
            return this._constructFromRawData(repository, data);
        });
    }

    static _constructFromRawData(repository, data)
    {
        return data['commits'].map((rawData) => {
            rawData.repository = repository;
            return CommitLog.ensureSingleton(rawData.id, rawData);
        });
    }
}

if (typeof module != 'undefined')
    module.exports.CommitLog = CommitLog;
