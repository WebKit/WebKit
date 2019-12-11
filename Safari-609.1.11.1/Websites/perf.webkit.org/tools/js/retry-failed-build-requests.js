async function createAdditionalBuildRequestsForTestGroupsWithFailedRequests(testGroups, maximumRetryFactor)
{
    for (const testGroup of testGroups) {
        if (!testGroup.mayNeedMoreRequests())
            continue;

        if (testGroup.isHidden()) {
            await testGroup.clearMayNeedMoreBuildRequests();
            continue;
        }

        let maxMissingBuildRequestCount = 0;
        let hasCompletedBuildRequestForEachCommitSet = true;
        let hasUnfinishedBuildRequest = false;
        for (const commitSet of testGroup.requestedCommitSets()) {
            const buildRequests = testGroup.requestsForCommitSet(commitSet).filter((buildRequest) => buildRequest.isTest());

            const completedBuildRequestCount = buildRequests.filter((buildRequest) => buildRequest.hasCompleted()).length;
            if (!completedBuildRequestCount)
                hasCompletedBuildRequestForEachCommitSet = false;

            hasCompletedBuildRequestForEachCommitSet &= (completedBuildRequestCount > 0);
            const unfinishedBuildRequestCount = buildRequests.filter((buildRequest) => !buildRequest.hasFinished()).length;
            // "potentiallySuccessfulCount" might be larger than testGroup.initialRepetitionCount() as user may add build requests manually.
            const potentiallySuccessfulCount = completedBuildRequestCount + unfinishedBuildRequestCount;
            maxMissingBuildRequestCount = Math.max(maxMissingBuildRequestCount, testGroup.initialRepetitionCount() - potentiallySuccessfulCount);

            if (unfinishedBuildRequestCount)
                hasUnfinishedBuildRequest = true;
        }
        const willExceedMaximumRetry = testGroup.repetitionCount() + maxMissingBuildRequestCount > maximumRetryFactor * testGroup.initialRepetitionCount();

        console.assert(maxMissingBuildRequestCount <= testGroup.initialRepetitionCount());
        if (maxMissingBuildRequestCount && !willExceedMaximumRetry && hasUnfinishedBuildRequest && !hasCompletedBuildRequestForEachCommitSet)
            continue;

        if (maxMissingBuildRequestCount && !willExceedMaximumRetry && hasCompletedBuildRequestForEachCommitSet) {
            await testGroup.addMoreBuildRequests(maxMissingBuildRequestCount);
            const analysisTask = await testGroup.fetchTask();
            console.log(`Added ${maxMissingBuildRequestCount} build request(s) to "${testGroup.name()}" of analysis task: ${analysisTask.id()} - "${analysisTask.name()}"`);
        }
        await testGroup.clearMayNeedMoreBuildRequests();
    }
}

if (typeof module !== 'undefined')
    module.exports.createAdditionalBuildRequestsForTestGroupsWithFailedRequests = createAdditionalBuildRequestsForTestGroupsWithFailedRequests;