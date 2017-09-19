'use strict';

class CommitSet extends DataModelObject {

    constructor(id, object)
    {
        super(id);
        this._repositories = [];
        this._repositoryToCommitMap = new Map;
        this._repositoryToPatchMap = new Map;
        this._repositoryToRootMap = new Map;
        this._repositoryToCommitOwnerMap = new Map;
        this._repositoryRequiresBuildMap = new Map;
        this._latestCommitTime = null;
        this._customRoots = [];
        this._allRootFiles = [];

        if (!object)
            return;

        this._updateFromObject(object);
    }

    updateSingleton(object)
    {
        this._repositoryToCommitMap.clear();
        this._repositoryToPatchMap.clear();
        this._repositoryToRootMap.clear();
        this._repositoryToCommitOwnerMap.clear();
        this._repositoryRequiresBuildMap.clear();
        this._repositories = [];
        this._updateFromObject(object);
    }

    _updateFromObject(object)
    {
        const rootFiles = new Set;
        for (const item of object.revisionItems) {
            const commit = item.commit;
            console.assert(commit instanceof CommitLog);
            console.assert(!item.patch || item.patch instanceof UploadedFile);
            console.assert(!item.rootFile || item.rootFile instanceof UploadedFile);
            console.assert(!item.commitOwner || item.commitOwner instanceof CommitLog);
            const repository = commit.repository();
            this._repositoryToCommitMap.set(repository, commit);
            this._repositoryToPatchMap.set(repository, item.patch);
            this._repositoryToCommitOwnerMap.set(repository, item.commitOwner);
            this._repositoryRequiresBuildMap.set(repository, item.requiresBuild);
            this._repositoryToRootMap.set(repository, item.rootFile);
            if (item.rootFile)
                rootFiles.add(item.rootFile);
            this._repositories.push(commit.repository());
        }
        this._customRoots = object.customRoots;
        this._allRootFiles = Array.from(rootFiles).concat(object.customRoots);
    }

    repositories() { return this._repositories; }
    customRoots() { return this._customRoots; }
    allRootFiles() { return this._allRootFiles; }
    commitForRepository(repository) { return this._repositoryToCommitMap.get(repository); }

    revisionForRepository(repository)
    {
        var commit = this._repositoryToCommitMap.get(repository);
        return commit ? commit.revision() : null;
    }

    ownerRevisionForRepository(repository)
    {
        const commit = this._repositoryToCommitOwnerMap.get(repository);
        return commit ? commit.revision() : null;
    }

    patchForRepository(repository) { return this._repositoryToPatchMap.get(repository); }
    rootForRepository(repository) { return this._repositoryToRootMap.get(repository); }
    requiresBuildForRepository(repository) { return this._repositoryRequiresBuildMap.get(repository); }

    // FIXME: This should return a Date object.
    latestCommitTime()
    {
        if (this._latestCommitTime == null) {
            var maxTime = 0;
            for (const [repository, commit] of this._repositoryToCommitMap)
                maxTime = Math.max(maxTime, +commit.time());
            this._latestCommitTime = maxTime;
        }
        return this._latestCommitTime;
    }

    equals(other)
    {
        if (this._repositories.length != other._repositories.length)
            return false;
        for (const [repository, commit] of this._repositoryToCommitMap) {
            if (commit != other._repositoryToCommitMap.get(repository))
                return false;
            if (this._repositoryToPatchMap.get(repository) != other._repositoryToPatchMap.get(repository))
                return false;
            if (this._repositoryToRootMap.get(repository) != other._repositoryToRootMap.get(repository))
                return false;
            if (this._repositoryToCommitOwnerMap.get(repository) != other._repositoryToCommitMap.get(repository))
                return false;
            if (this._repositoryRequiresBuildMap.get(repository) != other._repositoryRequiresBuildMap.get(repository))
                return false;
        }
        return CommitSet.areCustomRootsEqual(this._customRoots, other._customRoots);
    }

    static areCustomRootsEqual(customRoots1, customRoots2)
    {
        if (customRoots1.length != customRoots2.length)
            return false;
        const set2 = new Set(customRoots2);
        for (let file of customRoots1) {
            if (!set2.has(file))
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

            // FIXME: Add a flag to remember the fact this commit log is incomplete.
            const commit = CommitLog.ensureSingleton(commitId, {repository: repository, revision: revision, time: time});
            this._repositoryToCommitMap.set(repository, commit);
            this._repositories.push(repository);
        }
    }

    // Use CommitSet's static maps because MeasurementCommitSet and CommitSet are logically of the same type.
    // FIXME: Ideally, DataModel should take care of this but traversing prototype chain is expensive.
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
        this._customRoots = [];
    }

    setRevisionForRepository(repository, revision, patch = null, ownerRevision = null)
    {
        console.assert(repository instanceof Repository);
        console.assert(!patch || patch instanceof UploadedFile);
        this._revisionListByRepository.set(repository, {revision, patch, ownerRevision});
    }

    equals(other)
    {
        console.assert(other instanceof CustomCommitSet);
        if (this._revisionListByRepository.size != other._revisionListByRepository.size)
            return false;

        for (const [repository, thisRevision] of this._revisionListByRepository) {
            const otherRevision = other._revisionListByRepository.get(repository);
            if (!thisRevision != !otherRevision)
                return false;
            if (thisRevision && (thisRevision.revision != otherRevision.revision
                || thisRevision.patch != otherRevision.patch
                || thisRevision.ownerRevision != otherRevision.ownerRevision))
                return false;
        }
        return CommitSet.areCustomRootsEqual(this._customRoots, other._customRoots);
    }

    repositories() { return Array.from(this._revisionListByRepository.keys()); }
    revisionForRepository(repository)
    {
        const entry = this._revisionListByRepository.get(repository);
        if (!entry)
            return null;
        return entry.revision;
    }
    patchForRepository(repository)
    {
        const entry = this._revisionListByRepository.get(repository);
        if (!entry)
            return null;
        return entry.patch;
    }
    ownerRevisionForRepository(repository)
    {
        const entry = this._revisionListByRepository.get(repository);
        if (!entry)
            return null;
        return entry.ownerRevision;
    }
    customRoots() { return this._customRoots; }

    addCustomRoot(uploadedFile)
    {
        console.assert(uploadedFile instanceof UploadedFile);
        this._customRoots.push(uploadedFile);
    }
}

if (typeof module != 'undefined') {
    module.exports.CommitSet = CommitSet;
    module.exports.MeasurementCommitSet = MeasurementCommitSet;
    module.exports.CustomCommitSet = CustomCommitSet;
}
