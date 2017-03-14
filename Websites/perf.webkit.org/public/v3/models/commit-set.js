'use strict';

class CommitSet extends DataModelObject {

    constructor(id, object)
    {
        super(id);
        this._repositories = [];
        this._repositoryToCommitMap = {};
        this._latestCommitTime = null;

        if (!object)
            return;

        for (let row of object.commits) {
            const repositoryId = row.repository.id();
            console.assert(!this._repositoryToCommitMap[repositoryId]);
            this._repositoryToCommitMap[repositoryId] = CommitLog.ensureSingleton(row.id, row);
            this._repositories.push(row.repository);
        }
    }

    repositories() { return this._repositories; }
    commitForRepository(repository) { return this._repositoryToCommitMap[repository.id()]; }

    revisionForRepository(repository)
    {
        var commit = this._repositoryToCommitMap[repository.id()];
        return commit ? commit.revision() : null;
    }

    // FIXME: This should return a Date object.
    latestCommitTime()
    {
        if (this._latestCommitTime == null) {
            var maxTime = 0;
            for (var repositoryId in this._repositoryToCommitMap)
                maxTime = Math.max(maxTime, +this._repositoryToCommitMap[repositoryId].time());
            this._latestCommitTime = maxTime;
        }
        return this._latestCommitTime;
    }

    equals(other)
    {
        if (this._repositories.length != other._repositories.length)
            return false;
        for (var repositoryId in this._repositoryToCommitMap) {
            if (this._repositoryToCommitMap[repositoryId] != other._repositoryToCommitMap[repositoryId])
                return false;
        }
        return true;
    }

    static containsMultipleCommitsForRepository(commitSets, repository)
    {
        console.assert(repository instanceof Repository);
        if (commitSets.length < 2)
            return false;
        const firstCommit = commitSets[0].commitForRepository(repository);
        for (let set of commitSets) {
            const anotherCommit = set.commitForRepository(repository);
            if (!firstCommit != !anotherCommit || (firstCommit && firstCommit.revision() != anotherCommit.revision()))
                return true;
        }
        return false;
    }
}

class MeasurementCommitSet extends CommitSet {

    constructor(id, revisionList)
    {
        super(id, null);
        for (var values of revisionList) {
            // [<commit-id>, <repository-id>, <revision>, <time>]
            var commitId = values[0];
            var repositoryId = values[1];
            var revision = values[2];
            var time = values[3];
            var repository = Repository.findById(repositoryId);
            if (!repository)
                continue;

            this._repositoryToCommitMap[repositoryId] = CommitLog.ensureSingleton(commitId, {repository: repository, revision: revision, time: time});
            this._repositories.push(repository);
        }
    }

    // Use CommitSet's static maps because MeasurementCommitSet and CommitSet are logically of the same type.
    // FIXME: Idaelly, DataModel should take care of this but traversing prototype chain is expensive.
    namedStaticMap(name) { return CommitSet.namedStaticMap(name); }
    ensureNamedStaticMap(name) { return CommitSet.ensureNamedStaticMap(name); }
    static namedStaticMap(name) { return CommitSet.namedStaticMap(name); }
    static ensureNamedStaticMap(name) { return CommitSet.ensureNamedStaticMap(name); }

    static ensureSingleton(measurementId, revisionList)
    {
        const commitSetId = measurementId + '-commitset';
        return CommitSet.findById(commitSetId) || (new MeasurementCommitSet(commitSetId, revisionList));
    }
}

class CustomCommitSet {

    constructor()
    {
        this._revisionListByRepository = new Map;
    }

    setRevisionForRepository(repository, revision)
    {
        console.assert(repository instanceof Repository);
        this._revisionListByRepository.set(repository, revision);
    }

    repositories() { return Array.from(this._revisionListByRepository.keys()); }
    revisionForRepository(repository) { return this._revisionListByRepository.get(repository); }

}

if (typeof module != 'undefined') {
    module.exports.CommitSet = CommitSet;
    module.exports.MeasurementCommitSet = MeasurementCommitSet;
    module.exports.CustomCommitSet = CustomCommitSet;
}
