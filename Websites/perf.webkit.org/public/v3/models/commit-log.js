'use strict';

class CommitLog extends DataModelObject {
    constructor(id, rawData)
    {
        console.assert(parseInt(id) == id);
        super(id);
        console.assert(id == rawData.id)
        this._repository = rawData.repository;
        console.assert(this._repository instanceof Repository);
        this._rawData = rawData;
        this._ownedCommits = null;
        this._ownerCommit = null;
        this._ownedCommitByOwnedRepository = new Map;
    }

    updateSingleton(rawData)
    {
        super.updateSingleton(rawData);

        console.assert(+this._rawData['time'] == +rawData['time']);
        console.assert(this._rawData['revision'] == rawData['revision']);
        console.assert(this._rawData['revisionIdentifier'] == rawData['revisionIdentifier']);

        if (rawData.authorName)
            this._rawData.authorName = rawData.authorName;
        if (rawData.message)
            this._rawData.message = rawData.message;
        if (rawData.ownsCommits)
            this._rawData.ownsCommits = rawData.ownsCommits;
        if (rawData.order)
            this._rawData.order = rawData.order;
        if (rawData.testability)
            this._rawData.testability = rawData.testability;
    }

    repository() { return this._repository; }
    time() { return new Date(this._rawData['time']); }
    hasCommitTime() { return this._rawData['time'] > 0 && this._rawData['time'] != null; }
    testability() { return this._rawData['testability']; }
    author() { return this._rawData['authorName']; }
    revision() { return this._rawData['revision']; }
    revisionIdentifier() { return this._rawData['revisionIdentifier']; }
    message() { return this._rawData['message']; }
    url() { return this._repository.urlForRevision(this._rawData['revision']); }
    ownsCommits() { return this._rawData['ownsCommits']; }
    ownedCommits() { return this._ownedCommits; }
    ownerCommit() { return this._ownerCommit; }
    order() { return this._rawData['order']; }
    hasCommitOrder() { return this._rawData['order'] != null; }
    setOwnerCommits(ownerCommit) { this._ownerCommit = ownerCommit; }

    label() { return CommitLog._formatttedRevision(this.revision(), this.revisionIdentifier()); }

    static _repositoryType(revision)
    {
        if (parseInt(revision) == revision)
            return 'svn';
        if (revision.length == 40)
            return 'git';
        return null;
    }

    static _formatttedRevision(revision, revisionIdentifier = null) 
    {
        const formattedRevision = (() => {
            switch (this._repositoryType(revision)) {
            case 'svn':
                return 'r' + revision; // e.g. r12345
            case 'git':
                return revision.substring(0, 12);
            }
            return revision;
        })();
        if (revisionIdentifier)
            return `${revisionIdentifier} (${formattedRevision})`;
        return formattedRevision;
    }

    title() { return this._repository.name() + ' at ' + this.label(); }

    diff(previousCommit)
    {
        if (this == previousCommit)
            previousCommit = null;

        const repository = this._repository;
        if (!previousCommit)
            return {repository: repository, label: this.label(), url: this.url()};

        const toRevision = this.revision();
        const fromRevision = previousCommit.revision();
        const identifierPattern = /(?<number>\d+)@(?<branch>[\w\.\-]+)/;
        const repositoryType = CommitLog._repositoryType(toRevision);
       
        const label = ((fromMatch, toMatch) => {
            const separator = repositoryType == 'git' ? '..' : (repositoryType == 'svn' ? '-' : ' - ');
            const revisionRange = `${CommitLog._formatttedRevision(fromRevision)}${separator}${CommitLog._formatttedRevision(toRevision)}`;
            if (fromMatch && toMatch) {
                console.assert(fromMatch.groups.branch == toMatch.groups.branch);
                return `${fromMatch.groups.number}-${toMatch.groups.number}@${fromMatch.groups.branch} (${revisionRange})`;
            }
            if (fromMatch || toMatch)
                return `${previousCommit.label()} - ${this.label()}`;
            return revisionRange;
        })(identifierPattern.exec(previousCommit.revisionIdentifier()), identifierPattern.exec(this.revisionIdentifier()));

        return {repository, label, url: repository.urlForRevisionRange(fromRevision, toRevision)};
    }

    static fetchLatestCommitForPlatform(repository, platform)
    {
        console.assert(repository instanceof Repository);
        console.assert(platform instanceof Platform);
        return this.cachedFetch(`/api/commits/${repository.id()}/latest`, {platform: platform.id()}).then((data) => {
            const commits = data['commits'];
            if (!commits || !commits.length)
                return null;
            const rawData = commits[0];
            rawData.repository = repository;
            return CommitLog.ensureSingleton(rawData.id, rawData);
        });
    }

    static hasOrdering(firstCommit, secondCommit)
    {
        return (firstCommit.hasCommitTime() && secondCommit.hasCommitTime()) ||
            (firstCommit.hasCommitOrder() && secondCommit.hasCommitOrder());
    }

    static orderTwoCommits(firstCommit, secondCommit)
    {
        console.assert(CommitLog.hasOrdering(firstCommit, secondCommit));
        const firstCommitSmaller = firstCommit.hasCommitTime() && secondCommit.hasCommitTime() ?
            firstCommit.time() < secondCommit.time() : firstCommit.order() < secondCommit.order();
        return firstCommitSmaller ? [firstCommit, secondCommit] : [secondCommit, firstCommit];
    }

    ownedCommitForOwnedRepository(ownedRepository) { return this._ownedCommitByOwnedRepository.get(ownedRepository); }

    fetchOwnedCommits()
    {
        if (!this.repository().ownedRepositories())
            return Promise.reject();

        if (!this.ownsCommits())
            return Promise.reject();

        if (this._ownedCommits)
            return Promise.resolve(this._ownedCommits);

        return CommitLog.cachedFetch(`../api/commits/${this.repository().id()}/owned-commits?owner-revision=${escape(this.revision())}`).then((data) => {
            this._ownedCommits = CommitLog._constructFromRawData(data);
            this._ownedCommits.forEach((ownedCommit) => {
                ownedCommit.setOwnerCommits(this);
                this._ownedCommitByOwnedRepository.set(ownedCommit.repository(), ownedCommit);
            });
            return this._ownedCommits;
        });
    }

    _buildOwnedCommitMap()
    {
        const ownedCommitMap = new Map;
        for (const commit of this._ownedCommits)
            ownedCommitMap.set(commit.repository(), commit);
        return ownedCommitMap;
    }

    static ownedCommitDifferenceForOwnerCommits(...commits)
    {
        console.assert(commits.length >= 2);

        const ownedCommitRepositories = new Set;
        const ownedCommitMapList = commits.map((commit) => {
            console.assert(commit);
            console.assert(commit._ownedCommits);
            const ownedCommitMap = commit._buildOwnedCommitMap();
            for (const repository of ownedCommitMap.keys())
                ownedCommitRepositories.add(repository);
            return ownedCommitMap;
        });

        const difference = new Map;
        ownedCommitRepositories.forEach((ownedCommitRepository) => {
            const ownedCommits = ownedCommitMapList.map((ownedCommitMap) => ownedCommitMap.get(ownedCommitRepository));
            const uniqueOwnedCommits = new Set(ownedCommits);
            if (uniqueOwnedCommits.size > 1)
                difference.set(ownedCommitRepository, ownedCommits);
        });
        return difference;
    }

    static async fetchBetweenRevisions(repository, precedingRevision, lastRevision)
    {
        // FIXME: The cache should be smarter about fetching a range within an already fetched range, etc...
        // FIXME: We should evict some entries from the cache in cachedFetch.
        const data = await this.cachedFetch(`/api/commits/${repository.id()}/`, {precedingRevision, lastRevision});
        return this._constructFromRawData(data);
    }

    static async fetchForSingleRevision(repository, revision, prefixMatch=false)
    {
        const commit = repository.commitForRevision(revision);
        if (commit)
            return [commit];

        let params = {};
        if (prefixMatch)
            params['prefix-match'] = true;
        const data = await this.cachedFetch(`/api/commits/${repository.id()}/${revision}`, params);
        return this._constructFromRawData(data);
    }

    static _constructFromRawData(data)
    {
        return data['commits'].map((rawData) => {
            const repositoryId = rawData.repository;
            const repository = Repository.findById(repositoryId);
            rawData.repository = repository;
            const commit = CommitLog.ensureSingleton(rawData.id, rawData);
            repository.setCommitForRevision(commit.revision(), commit);
            return commit;
        });
    }
}

if (typeof module != 'undefined')
    module.exports.CommitLog = CommitLog;
