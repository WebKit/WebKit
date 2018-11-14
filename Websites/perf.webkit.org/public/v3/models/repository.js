'use strict';

class Repository extends LabeledObject {
    constructor(id, object)
    {
        super(id, object);
        this._url = object.url;
        this._blameUrl = object.blameUrl;
        this._hasReportedCommits = object.hasReportedCommits;
        this._ownerId = object.owner;
        this._commitByRevision = new Map;

        if (!this._ownerId)
            this.ensureNamedStaticMap('topLevelName')[this.name()] = this;
        else {
            const ownerships = this.ensureNamedStaticMap('repositoryOwnerships');
            if (!(this._ownerId in ownerships))
                ownerships[this._ownerId] = [this];
            else
                ownerships[this._ownerId].push(this);
        }
    }

    static findTopLevelByName(name)
    {
        const map = this.namedStaticMap('topLevelName');
        return map ? map[name] : null;
    }

    commitForRevision(revision) { return this._commitByRevision.get(revision); }
    setCommitForRevision(revision, commit) { this._commitByRevision.set(revision, commit); }
    hasUrlForRevision() { return !!this._url; }

    urlForRevision(currentRevision)
    {
        return (this._url || '').replace(/\$1/g, currentRevision);
    }

    urlForRevisionRange(from, to)
    {
        return (this._blameUrl || '').replace(/\$1/g, from).replace(/\$2/g, to);
    }

    ownerId()
    {
        return this._ownerId;
    }

    ownedRepositories()
    {
        const ownerships = this.namedStaticMap('repositoryOwnerships');
        return ownerships ? ownerships[this.id()] : null;
    }

    static sortByNamePreferringOnesWithURL(repositories)
    {
        return repositories.sort(function (a, b) {
            if (!!a._blameUrl == !!b._blameUrl) {
                if (a.name() > b.name())
                    return 1;
                else if (a.name() < b.name())
                    return -1;
                return 0;
            } else if (b._blameUrl) // a > b
                return 1;
            return -1;
        });
    }

}

if (typeof module != 'undefined')
    module.exports.Repository = Repository;
