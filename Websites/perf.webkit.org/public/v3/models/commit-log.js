
class CommitLog extends DataModelObject {
    constructor(id, repository, rawData)
    {
        super(id);
        this._repository = repository;
        this._rawData = rawData;
    }

    static ensureSingleton(repository, rawData)
    {
        var id = repository.id() + '-' + rawData['revision'];
        var singleton = this.findById(id);
        if (singleton) {
            if (rawData.authorName)
                singleton._rawData.authorName = rawData.authorName;
            if (rawData.message)
                singleton._rawData.message = rawData.message;
            return singleton;
        }
        return new CommitLog(id, repository, rawData);
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

    static fetchBetweenRevisions(repository, from, to)
    {
        var params = [];
        if (from && to) {
            params.push(['from', from]);
            params.push(['to', to]);
        }

        var url = '../api/commits/' + repository.id() + '/?' + params.map(function (keyValue) {
            return encodeURIComponent(keyValue[0]) + '=' + encodeURIComponent(keyValue[1]);
        }).join('&');


        var cachedLogs = this._cachedCommitLogs(repository, from, to);
        if (cachedLogs)
            return new Promise(function (resolve) { resolve(cachedLogs); });

        var self = this;
        return getJSONWithStatus(url).then(function (data) {
            var commits = data['commits'].map(function (rawData) { return CommitLog.ensureSingleton(repository, rawData); });
            self._cacheCommitLogs(repository, from, to, commits);
            return commits;
        });
    }

    static _cachedCommitLogs(repository, from, to)
    {
        if (!this._caches)
            return null;
        var cache = this._caches[repository.id()];
        if (!cache)
            return null;
        // FIXME: Make each commit know of its parent, then we can do a better caching. 
        return cache[from + '|' + to];
    }

    static _cacheCommitLogs(repository, from, to, logs)
    {
        if (!this._caches)
            this._caches = {};
        if (!this._caches[repository.id()])
            this._caches[repository.id()] = {};
        this._caches[repository.id()][from + '|' + to] = logs;
    }
}
