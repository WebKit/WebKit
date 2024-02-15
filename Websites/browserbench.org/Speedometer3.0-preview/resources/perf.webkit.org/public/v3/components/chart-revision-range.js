
class ChartRevisionRange {

    constructor(chart, metric)
    {
        this._chart = chart;

        const thisClass = new.target;
        this._computeRevisionList = new LazilyEvaluatedFunction((currentPoint, prevoiusPoint) => {
            return thisClass._computeRevisionList(currentPoint, prevoiusPoint);
        });

        this._computeRevisionRange = new LazilyEvaluatedFunction((repository, currentPoint, previousPoint) => {
            return {
                repository,
                from: thisClass._revisionForPoint(repository, previousPoint),
                to: thisClass._revisionForPoint(repository, currentPoint)};
        });
    }

    revisionList()
    {
        const referencePoints = this._chart.referencePoints('current');
        const currentPoint = referencePoints ? referencePoints.currentPoint : null;
        const previousPoint = referencePoints ? referencePoints.previousPoint : null;
        return this._computeRevisionList.evaluate(currentPoint, previousPoint);
    }

    rangeForRepository(repository)
    {
        const referencePoints = this._chart.referencePoints('current');
        const currentPoint = referencePoints ? referencePoints.currentPoint : null;
        const previousPoint = referencePoints ? referencePoints.previousPoint : null;
        return this._computeRevisionRange.evaluate(repository, currentPoint, previousPoint);
    }

    static _revisionForPoint(repository, point)
    {
        if (!point || !repository)
            return null;
        const commitSet = point.commitSet();
        if (!commitSet)
            return null;
        const commit = commitSet.commitForRepository(repository);
        if (!commit)
            return null;
        return commit.revision();
    }

    static _computeRevisionList(currentPoint, previousPoint)
    {
        if (!currentPoint)
            return null;

        const currentCommitSet = currentPoint.commitSet();
        const previousCommitSet = previousPoint ? previousPoint.commitSet() : null;

        const repositoriesInCurrentCommitSet = Repository.sortByNamePreferringOnesWithURL(currentCommitSet.repositories());
        const revisionList = [];
        for (let repository of repositoriesInCurrentCommitSet) {
            let currentCommit = currentCommitSet.commitForRepository(repository);
            let previousCommit = previousCommitSet ? previousCommitSet.commitForRepository(repository) : null;
            revisionList.push(currentCommit.diff(previousCommit));
        }
        return revisionList;
    }

}
