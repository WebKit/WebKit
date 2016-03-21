'use strict';

class RootSet extends DataModelObject {

    constructor(id, object)
    {
        super(id);
        this._repositories = [];
        this._repositoryToCommitMap = {};
        this._latestCommitTime = null;

        if (!object)
            return;

        for (var row of object.roots) {
            var repositoryId = row.repository;
            row.repository = Repository.findById(repositoryId);

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

    static containsMultipleCommitsForRepository(rootSets, repository)
    {
        console.assert(repository instanceof Repository);
        if (rootSets.length < 2)
            return false;
        var firstCommit = rootSets[0].commitForRepository(repository);
        for (var set of rootSets) {
            var anotherCommit = set.commitForRepository(repository);
            if (!firstCommit != !anotherCommit || (firstCommit && firstCommit.revision() != anotherCommit.revision()))
                return true;
        }
        return false;
    }
}

class MeasurementRootSet extends RootSet {

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

    static ensureSingleton(measurementId, revisionList)
    {
        var rootSetId = measurementId + '-rootset';
        return RootSet.findById(rootSetId) || (new MeasurementRootSet(rootSetId, revisionList));
    }
}

class CustomRootSet {

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
    module.exports.RootSet = RootSet;
    module.exports.MeasurementRootSet = MeasurementRootSet;
    module.exports.CustomRootSet = CustomRootSet;
}
