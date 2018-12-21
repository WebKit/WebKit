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
        this._ownerRepositoryToOwnedRepositoriesMap = new Map;
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
        this._ownerRepositoryToOwnedRepositoriesMap.clear();
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
            if (item.commitOwner) {
                this._repositoryToCommitOwnerMap.set(repository, item.commitOwner);
                const ownerRepository = item.commitOwner.repository();
                if (!this._ownerRepositoryToOwnedRepositoriesMap.get(ownerRepository))
                    this._ownerRepositoryToOwnedRepositoriesMap.set(ownerRepository, [repository]);
                else
                    this._ownerRepositoryToOwnedRepositoriesMap.get(ownerRepository).push(repository);
            }
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
    ownerCommitForRepository(repository) { return this._repositoryToCommitOwnerMap.get(repository); }
    topLevelRepositories() { return Repository.sortByNamePreferringOnesWithURL(this._repositories.filter((repository) => !this.ownerRevisionForRepository(repository))); }
    ownedRepositoriesForOwnerRepository(repository) { return this._ownerRepositoryToOwnedRepositoriesMap.get(repository); }
    commitsWithTestability() { return this.commits().filter((commit) => !!commit.testability()); }
    commits() { return  Array.from(this._repositoryToCommitMap.values()); }

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
    requiresBuildForRepository(repository) { return this._repositoryRequiresBuildMap.get(repository) || false; }

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
            if (this.patchForRepository(repository) != other.patchForRepository(repository))
                return false;
            if (this.rootForRepository(repository) != other.rootForRepository(repository))
                return false;
            if (this.ownerCommitForRepository(repository) != other.ownerCommitForRepository(repository))
                return false;
            if (this.requiresBuildForRepository(repository) != other.requiresBuildForRepository(repository))
                return false;
        }
        return CommitSet.areCustomRootsEqual(this._customRoots, other._customRoots);
    }

    hasSameRepositories(commitSet)
    {
        return commitSet.repositories().length === this._repositoryToCommitMap.size
            && commitSet.repositories().every((repository) => this._repositoryToCommitMap.has(repository));
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

    containsRootOrPatchOrOwnedCommit()
    {
        if (this.allRootFiles().length)
            return true;

        for (const repository of this.repositories()) {
            if (this.ownerCommitForRepository(repository))
                return true;
            if (this.ownedRepositoriesForOwnerRepository(repository))
                return true;
            if (this.patchForRepository(repository))
                return true;
        }
        return false;
    }

    static createNameWithoutCollision(name, existingNameSet)
    {
        console.assert(existingNameSet instanceof Set);
        if (!existingNameSet.has(name))
            return name;
        const nameWithNumberMatch = name.match(/(.+?)\s*\(\s*(\d+)\s*\)\s*$/);
        let number = 1;
        if (nameWithNumberMatch) {
            name = nameWithNumberMatch[1];
            number = parseInt(nameWithNumberMatch[2]);
        }

        let newName;
        do {
            number++;
            newName = `${name} (${number})`;
        } while (existingNameSet.has(newName));

        return newName;
    }

    static diff(firstCommitSet, secondCommitSet)
    {
        console.assert(!firstCommitSet.equals(secondCommitSet));
        const allRepositories = new Set([...firstCommitSet.repositories(), ...secondCommitSet.repositories()]);
        const sortedRepositories = Repository.sortByNamePreferringOnesWithURL([...allRepositories]);
        const nameParts = [];
        const missingCommit = {label: () => 'none'};
        const missingPatch = {filename: () => 'none'};
        const makeNameGenerator = () => {
            const existingNameSet = new Set;
            return (name) => {
                const newName = CommitSet.createNameWithoutCollision(name, existingNameSet);
                existingNameSet.add(newName);
                return newName;
            }
        };

        for (const repository of sortedRepositories) {
            const firstCommit = firstCommitSet.commitForRepository(repository) || missingCommit;
            const secondCommit = secondCommitSet.commitForRepository(repository) || missingCommit;
            const firstPatch = firstCommitSet.patchForRepository(repository) || missingPatch;
            const secondPatch = secondCommitSet.patchForRepository(repository) || missingPatch;
            const nameGenerator = makeNameGenerator();

            if (firstCommit == secondCommit && firstPatch == secondPatch)
                continue;

            if (firstCommit != secondCommit && firstPatch == secondPatch)
                nameParts.push(`${repository.name()}: ${secondCommit.diff(firstCommit).label}`);

            // FIXME: It would be nice if we can abbreviate the name when it's too long.
            const nameForFirstPatch = nameGenerator(firstPatch.filename());
            const nameForSecondPath = nameGenerator(secondPatch.filename());

            if (firstCommit == secondCommit && firstPatch != secondPatch)
                nameParts.push(`${repository.name()}: ${nameForFirstPatch} - ${nameForSecondPath}`);

            if (firstCommit != secondCommit && firstPatch != secondPatch)
                nameParts.push(`${repository.name()}: ${firstCommit.label()} with ${nameForFirstPatch} - ${secondCommit.label()} with ${nameForSecondPath}`);
        }

        if (firstCommitSet.allRootFiles().length || secondCommitSet.allRootFiles().length) {
            const firstRootFileSet = new Set(firstCommitSet.allRootFiles());
            const secondRootFileSet = new Set(secondCommitSet.allRootFiles());
            const uniqueInFirstCommitSet = firstCommitSet.allRootFiles().filter((rootFile) => !secondRootFileSet.has(rootFile));
            const uniqueInSecondCommitSet = secondCommitSet.allRootFiles().filter((rootFile) => !firstRootFileSet.has(rootFile));
            const nameGenerator = makeNameGenerator();
            const firstDescription = uniqueInFirstCommitSet.map((rootFile) => nameGenerator(rootFile.filename())).join(', ');
            const secondDescription = uniqueInSecondCommitSet.map((rootFile) => nameGenerator(rootFile.filename())).join(', ');
            nameParts.push(`Roots: ${firstDescription || 'none'} - ${secondDescription || 'none'}`);
        }

        return nameParts.join(' ');
    }

    static revisionSetsFromCommitSets(commitSets)
    {
        return commitSets.map((commitSet) => {
            console.assert(commitSet instanceof CustomCommitSet || commitSet instanceof CommitSet);
            const revisionSet = {};
            for (let repository of commitSet.repositories()) {
                const patchFile = commitSet.patchForRepository(repository);
                revisionSet[repository.id()] = {
                    revision: commitSet.revisionForRepository(repository),
                    ownerRevision: commitSet.ownerRevisionForRepository(repository),
                    patch: patchFile ? patchFile.id() : null,
                };
            }
            const customRoots = commitSet.customRoots();
            if (customRoots && customRoots.length)
                revisionSet['customRoots'] = customRoots.map((uploadedFile) => uploadedFile.id());
            return revisionSet;
        });
    }
}

class MeasurementCommitSet extends CommitSet {

    constructor(id, revisionList)
    {
        super(id, null);
        for (const values of revisionList) {
            // [<commit-id>, <repository-id>, <revision>, <order>, <time>]
            const commitId = values[0];
            const repositoryId = values[1];
            const revision = values[2];
            const order = values[3];
            const time = values[4];
            const repository = Repository.findById(repositoryId);
            if (!repository)
                continue;

            // FIXME: Add a flag to remember the fact this commit log is incomplete.
            const commit = CommitLog.ensureSingleton(commitId, {id: commitId, repository, revision, order, time});
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
    topLevelRepositories() { return Repository.sortByNamePreferringOnesWithURL(this.repositories().filter((repository) => !this.ownerRevisionForRepository(repository))); }
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

class IntermediateCommitSet {

    constructor(commitSet)
    {
        console.assert(commitSet instanceof CommitSet);
        this._commitByRepository = new Map;
        this._ownerToOwnedRepositories = new Map;
        this._fetchingPromiseByRepository = new Map;

        for (const repository of commitSet.repositories())
            this.setCommitForRepository(repository, commitSet.commitForRepository(repository), commitSet.ownerCommitForRepository(repository));
    }

    fetchCommitLogs()
    {
        const fetchingPromises = [];
        for (const [repository, commit] of this._commitByRepository)
            fetchingPromises.push(this._fetchCommitLogAndOwnedCommits(repository, commit.revision()));
        return Promise.all(fetchingPromises);
    }

    commitsWithTestabilityWarnings() { return this.commits().filter((commit) => !!commit.testabilityWarning()); }
    commits() { return  Array.from(this._commitByRepository.values()); }

    _fetchCommitLogAndOwnedCommits(repository, revision)
    {
        return CommitLog.fetchForSingleRevision(repository, revision).then((commits) => {
            console.assert(commits.length === 1);
            const commit = commits[0];
            if (!commit.ownsCommits())
                return commit;
            return commit.fetchOwnedCommits().then(() => commit);
        });
    }

    updateRevisionForOwnerRepository(repository, revision)
    {
        const fetchingPromise = this._fetchCommitLogAndOwnedCommits(repository, revision);
        this._fetchingPromiseByRepository.set(repository, fetchingPromise);
        return fetchingPromise.then((commit) => {
            const currentFetchingPromise = this._fetchingPromiseByRepository.get(repository);
            if (currentFetchingPromise !== fetchingPromise)
                return;
            this._fetchingPromiseByRepository.set(repository, null);
            this.setCommitForRepository(repository, commit);
        });
    }

    setCommitForRepository(repository, commit, ownerCommit = null)
    {
        console.assert(repository instanceof Repository);
        console.assert(commit instanceof CommitLog);
        this._commitByRepository.set(repository, commit);
        if (!ownerCommit)
            ownerCommit = commit.ownerCommit();
        if (ownerCommit) {
            const ownerRepository = ownerCommit.repository();
            if (!this._ownerToOwnedRepositories.has(ownerRepository))
                this._ownerToOwnedRepositories.set(ownerRepository, new Set);
            const repositorySet = this._ownerToOwnedRepositories.get(ownerRepository);
            repositorySet.add(repository);
        }
    }

    removeCommitForRepository(repository)
    {
        console.assert(repository instanceof Repository);
        this._fetchingPromiseByRepository.set(repository, null);
        const ownerCommit = this.ownerCommitForRepository(repository);
        if (ownerCommit) {
            const repositorySet = this._ownerToOwnedRepositories.get(ownerCommit.repository());
            console.assert(repositorySet.has(repository));
            repositorySet.delete(repository);
        } else if (this._ownerToOwnedRepositories.has(repository)) {
            const ownedRepositories = this._ownerToOwnedRepositories.get(repository);
            for (const ownedRepository of ownedRepositories)
                this._commitByRepository.delete(ownedRepository);
            this._ownerToOwnedRepositories.delete(repository);
        }
        this._commitByRepository.delete(repository);
    }

    ownsCommitsForRepository(repository) { return this.commitForRepository(repository).ownsCommits(); }

    repositories() { return Array.from(this._commitByRepository.keys()); }
    highestLevelRepositories() { return Repository.sortByNamePreferringOnesWithURL(this.repositories().filter((repository) => !this.ownerCommitForRepository(repository))); }
    commitForRepository(repository) { return this._commitByRepository.get(repository); }
    ownedRepositoriesForOwnerRepository(repository) { return this._ownerToOwnedRepositories.get(repository); }

    ownerCommitForRepository(repository)
    {
        const commit = this._commitByRepository.get(repository);
        if (!commit)
            return null;
        return commit.ownerCommit();
    }
}

if (typeof module != 'undefined') {
    module.exports.CommitSet = CommitSet;
    module.exports.MeasurementCommitSet = MeasurementCommitSet;
    module.exports.CustomCommitSet = CustomCommitSet;
    module.exports.IntermediateCommitSet = IntermediateCommitSet;
}
