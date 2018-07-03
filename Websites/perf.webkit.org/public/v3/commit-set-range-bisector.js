'use strict';


class CommitSetRangeBisector {
    static async commitSetClosestToMiddleOfAllCommits(commitSetsToSplit, availableCommitSets)
    {
        console.assert(commitSetsToSplit.length === 2);
        const [firstCommitSet, secondCommitSet] = commitSetsToSplit;
        if (firstCommitSet.containsRootOrPatchOrOwnedCommit() || secondCommitSet.containsRootOrPatchOrOwnedCommit())
            return null;

        if (!firstCommitSet.hasSameRepositories(secondCommitSet))
            return null;

        const repositoriesWithCommitTime = new Set;
        const commitRangeByRepository = new Map;
        const indexForAllTimelessCommitsWithOrderByRepository = new Map;
        const allCommitsWithCommitTime = [];
        const repositoriesWithoutOrdering = [];

        await Promise.all(firstCommitSet.topLevelRepositories().map(async (repository) => {
            const firstCommit = firstCommitSet.commitForRepository(repository);
            const secondCommit = secondCommitSet.commitForRepository(repository);

            if (!CommitLog.hasOrdering(firstCommit, secondCommit)) {
                repositoriesWithoutOrdering.push(repository);
                commitRangeByRepository.set((repository), (commit) => commit === firstCommit || commit === secondCommit);
                return;
            }

            const [startCommit, endCommit] = CommitLog.orderTwoCommits(firstCommit, secondCommit);
            const commitsExcludingStartCommit = startCommit === endCommit ? [] : await CommitLog.fetchBetweenRevisions(repository, startCommit.revision(), endCommit.revision());

            if (startCommit.hasCommitTime()) {
                allCommitsWithCommitTime.push(startCommit, ...commitsExcludingStartCommit);
                commitRangeByRepository.set(repository, (commit) =>
                    commit.hasCommitTime() && startCommit.time() <= commit.time() && commit.time() <= endCommit.time());
                repositoriesWithCommitTime.add(repository);
            } else {
                const indexByCommit = new Map;
                indexByCommit.set(startCommit, 0);
                commitsExcludingStartCommit.forEach((commit, index) => indexByCommit.set(commit, index + 1));
                indexForAllTimelessCommitsWithOrderByRepository.set(repository, indexByCommit);
                commitRangeByRepository.set(repository, (commit) =>
                    commit.hasCommitOrder() && startCommit.order() <= commit.order() && commit.order() <= endCommit.order());
            }
        }));

        if (!repositoriesWithCommitTime.size && !indexForAllTimelessCommitsWithOrderByRepository.size && !repositoriesWithoutOrdering.size)
            return null;

        const commitSetsInRange = this._findCommitSetsWithinRange(firstCommitSet, secondCommitSet, availableCommitSets, commitRangeByRepository);
        let sortedCommitSets = this._orderCommitSetsByTimeAndOrderThenDeduplicate(commitSetsInRange, repositoriesWithCommitTime, [...indexForAllTimelessCommitsWithOrderByRepository.keys()], repositoriesWithoutOrdering);
        if (!sortedCommitSets.length)
            return null;

        let remainingCommitSets = this._closestCommitSetsToBisectingCommitByTime(sortedCommitSets, repositoriesWithCommitTime, allCommitsWithCommitTime);
        remainingCommitSets = this._findCommitSetsClosestToMiddleOfCommitsWithOrder(remainingCommitSets, indexForAllTimelessCommitsWithOrderByRepository);

        if (!remainingCommitSets.length)
            return null;
        return remainingCommitSets[Math.floor(remainingCommitSets.length / 2)];
    }

    static _findCommitSetsWithinRange(firstCommitSetSpecifiedInRange, secondCommitSetSpecifiedInRange, availableCommitSets, commitRangeByRepository)
    {
        return availableCommitSets.filter((commitSet) => {
            if (!commitSet.hasSameRepositories(firstCommitSetSpecifiedInRange)
                || commitSet.equals(firstCommitSetSpecifiedInRange) || commitSet.equals(secondCommitSetSpecifiedInRange))
                return false;
            for (const [repository, isCommitInRange] of commitRangeByRepository) {
                const commit = commitSet.commitForRepository(repository);
                if (!isCommitInRange(commit))
                    return false;
            }
            return true;
        });
    }

    static _orderCommitSetsByTimeAndOrderThenDeduplicate(commitSets, repositoriesWithCommitTime, repositoriesWithCommitOrderOnly, repositoriesWithoutOrdering)
    {
        const sortedCommitSets = commitSets.sort((firstCommitSet, secondCommitSet) => {
            for (const repository of repositoriesWithCommitTime) {
                const firstCommit = firstCommitSet.commitForRepository(repository);
                const secondCommit = secondCommitSet.commitForRepository(repository);
                const diff = firstCommit.time() - secondCommit.time();
                if (!diff)
                    continue;
                return diff;
            }
            for (const repository of repositoriesWithCommitOrderOnly) {
                const firstCommit = firstCommitSet.commitForRepository(repository);
                const secondCommit = secondCommitSet.commitForRepository(repository);
                const diff = firstCommit.order() - secondCommit.order();
                if (!diff)
                    continue;
                return diff;
            }
            for (const repository of repositoriesWithoutOrdering) {
                const firstCommit = firstCommitSet.commitForRepository(repository);
                const secondCommit = secondCommitSet.commitForRepository(repository);
                if (firstCommit === secondCommit)
                    continue;
                return firstCommit.revision() < secondCommit.revision() ? -1 : 1;
            }
            return 0;
        });

        return sortedCommitSets.filter((currentSet, i) => !i || !currentSet.equals(sortedCommitSets[i - 1]));
    }

    static _closestCommitSetsToBisectingCommitByTime(sortedCommitSets, repositoriesWithCommitTime, allCommitsWithCommitTime)
    {
        if (!repositoriesWithCommitTime.size)
            return sortedCommitSets;

        const indexByCommitWithTime = new Map;
        allCommitsWithCommitTime.sort((firstCommit, secondCommit) => firstCommit.time() - secondCommit.time())
            .forEach((commit, index) => indexByCommitWithTime.set(commit, index));

        const commitToCommitSetMap = this._buildCommitToCommitSetMap(repositoriesWithCommitTime, sortedCommitSets);
        const closestCommit = this._findCommitClosestToMiddleIndex(indexByCommitWithTime, commitToCommitSetMap.keys());
        return Array.from(commitToCommitSetMap.get(closestCommit));
    }

    static _findCommitSetsClosestToMiddleOfCommitsWithOrder(remainingCommitSets, indexForAllTimelessCommitsWithOrderByRepository)
    {
        if (!indexForAllTimelessCommitsWithOrderByRepository.size)
            return remainingCommitSets;

        const commitWithOrderToCommitSets = this._buildCommitToCommitSetMap(indexForAllTimelessCommitsWithOrderByRepository.keys(), remainingCommitSets);

        for (const [repository, indexByCommit] of indexForAllTimelessCommitsWithOrderByRepository) {
            const commitsInRemainingSetsForCurrentRepository = remainingCommitSets.map((commitSet) => commitSet.commitForRepository(repository));
            const closestCommit = this._findCommitClosestToMiddleIndex(indexByCommit, commitsInRemainingSetsForCurrentRepository);
            const commitSetsContainingClosestCommit = commitWithOrderToCommitSets.get(closestCommit);
            remainingCommitSets = remainingCommitSets.filter((commitSet) => commitSetsContainingClosestCommit.has(commitSet));
            if (!remainingCommitSets.length)
                return remainingCommitSets;
        }
        return remainingCommitSets;
    }

    static _buildCommitToCommitSetMap(repositories, commitSets)
    {
        const commitToCommitSetMap = new Map;
        for (const repository of repositories) {
            for (const commitSet of commitSets) {
                const commit = commitSet.commitForRepository(repository);
                if (!commitToCommitSetMap.has(commit))
                    commitToCommitSetMap.set(commit, new Set);
                commitToCommitSetMap.get(commit).add(commitSet);
            }
        }
        return commitToCommitSetMap;
    }

    static _findCommitClosestToMiddleIndex(indexByCommit, commits)
    {
        const desiredCommitIndex = indexByCommit.size / 2;
        let minCommitDistance = indexByCommit.size;
        let closestCommit = null;
        for (const commit of commits) {
            const index = indexByCommit.get(commit);
            const distanceForCommit = Math.abs(index - desiredCommitIndex);
            if (distanceForCommit < minCommitDistance) {
                minCommitDistance = distanceForCommit;
                closestCommit = commit;
            }
        }
        return closestCommit;
    }
}

if (typeof module != 'undefined')
    module.exports.CommitSetRangeBisector = CommitSetRangeBisector;