'use strict';

class AnalysisTask extends LabeledObject {
    constructor(id, object)
    {
        super(id, object);
        this._author = object.author;
        this._createdAt = object.createdAt;

        console.assert(!object.platform || object.platform instanceof Platform);
        this._platform = object.platform;

        console.assert(!object.metric || object.metric instanceof Metric);
        this._metric = object.metric;

        this._startMeasurementId = object.startRun;
        this._startTime = object.startRunTime;
        this._endMeasurementId = object.endRun;
        this._endTime = object.endRunTime;
        this._category = object.category;
        this._changeType = object.result; // Can't change due to v2 compatibility.
        this._needed = object.needed;
        this._bugs = object.bugs || [];
        this._causes = object.causes || [];
        this._fixes = object.fixes || [];
        this._buildRequestCount = +object.buildRequestCount;
        this._finishedBuildRequestCount = +object.finishedBuildRequestCount;
    }

    static findByPlatformAndMetric(platformId, metricId)
    {
        return this.all().filter((task) => {
            const platform = task._platform;
            const metric = task._metric;
            return platform && metric && platform.id() == platformId && metric.id() == metricId;
        });
    }

    updateSingleton(object)
    {
        super.updateSingleton(object);

        console.assert(this._author == object.author);
        console.assert(+this._createdAt == +object.createdAt);
        console.assert(this._platform == object.platform);
        console.assert(this._metric == object.metric);
        console.assert(this._startMeasurementId == object.startRun);
        console.assert(this._startTime == object.startRunTime);
        console.assert(this._endMeasurementId == object.endRun);
        console.assert(this._endTime == object.endRunTime);

        this._category = object.category;
        this._changeType = object.result; // Can't change due to v2 compatibility.
        this._needed = object.needed;
        this._bugs = object.bugs || [];
        this._causes = object.causes || [];
        this._fixes = object.fixes || [];
        this._buildRequestCount = +object.buildRequestCount;
        this._finishedBuildRequestCount = +object.finishedBuildRequestCount;
    }

    isCustom() { return !this._platform; }
    hasResults() { return this._finishedBuildRequestCount; }
    hasPendingRequests() { return this._finishedBuildRequestCount < this._buildRequestCount; }
    requestLabel() { return `${this._finishedBuildRequestCount} of ${this._buildRequestCount}`; }

    startMeasurementId() { return this._startMeasurementId; }
    startTime() { return this._startTime; }
    endMeasurementId() { return this._endMeasurementId; }
    endTime() { return this._endTime; }

    author() { return this._author || ''; }
    createdAt() { return this._createdAt; }
    bugs() { return this._bugs; }
    causes() { return this._causes; }
    fixes() { return this._fixes; }
    platform() { return this._platform; }
    metric() { return this._metric; }

    changeType() { return this._changeType; }

    updateName(newName) { return this._updateRemoteState({name: newName}); }
    updateChangeType(changeType) { return this._updateRemoteState({result: changeType}); }

    _updateRemoteState(param)
    {
        param.task = this.id();
        return PrivilegedAPI.sendRequest('update-analysis-task', param).then(function (data) {
            return AnalysisTask.cachedFetch('/api/analysis-tasks', {id: param.task}, true)
                .then(AnalysisTask._constructAnalysisTasksFromRawData.bind(AnalysisTask));
        });
    }

    associateBug(tracker, bugNumber)
    {
        console.assert(tracker instanceof BugTracker);
        console.assert(typeof(bugNumber) == 'number');
        var id = this.id();
        return PrivilegedAPI.sendRequest('associate-bug', {
            task: id,
            bugTracker: tracker.id(),
            number: bugNumber,
        }).then(function (data) {
            return AnalysisTask.cachedFetch('/api/analysis-tasks', {id: id}, true)
                .then(AnalysisTask._constructAnalysisTasksFromRawData.bind(AnalysisTask));
        });
    }

    dissociateBug(bug)
    {
        console.assert(bug instanceof Bug);
        console.assert(this.bugs().includes(bug));
        var id = this.id();
        return PrivilegedAPI.sendRequest('associate-bug', {
            task: id,
            bugTracker: bug.bugTracker().id(),
            number: bug.bugNumber(),
            shouldDelete: true,
        }).then(function (data) {
            return AnalysisTask.cachedFetch('/api/analysis-tasks', {id: id}, true)
                .then(AnalysisTask._constructAnalysisTasksFromRawData.bind(AnalysisTask));
        });
    }

    associateCommit(kind, repository, revision)
    {
        console.assert(kind == 'cause' || kind == 'fix');
        console.assert(repository instanceof Repository);
        if (revision.startsWith('r'))
            revision = revision.substring(1);
        const id = this.id();
        return PrivilegedAPI.sendRequest('associate-commit', {
            task: id,
            repository: repository.id(),
            revision: revision,
            kind: kind,
        }).then((data) => {
            return AnalysisTask.cachedFetch('/api/analysis-tasks', {id}, true)
                .then(AnalysisTask._constructAnalysisTasksFromRawData.bind(AnalysisTask));
        });
    }

    dissociateCommit(commit)
    {
        console.assert(commit instanceof CommitLog);
        var id = this.id();
        return PrivilegedAPI.sendRequest('associate-commit', {
            task: id,
            commit: commit.id(),
        }).then(function (data) {
            return AnalysisTask.cachedFetch('/api/analysis-tasks', {id: id}, true)
                .then(AnalysisTask._constructAnalysisTasksFromRawData.bind(AnalysisTask));
        });
    }

    category()
    {
        var category = 'unconfirmed';

        if (this._changeType == 'unchanged' || this._changeType == 'inconclusive'
            || (this._changeType == 'regression' && this._fixes.length)
            || (this._changeType == 'progression' && (this._causes.length || this._fixes.length)))
            category = 'closed';
        else if (this._causes.length || this._fixes.length || this._changeType == 'regression' || this._changeType == 'progression')
            category = 'investigated';

        return category;
    }

    async commitSetsFromTestGroupsAndMeasurementSet()
    {

        const platform = this.platform();
        const metric = this.metric();
        if (!platform || !metric)
            return [];

        const lastModified = platform.lastModified(metric);
        const measurementSet = MeasurementSet.findSet(platform.id(), metric.id(), lastModified);
        const fetchingMeasurementSetPromise = measurementSet.fetchBetween(this.startTime(), this.endTime());

        const allTestGroupsInTask = await TestGroup.fetchForTask(this.id());
        const allCommitSetsInTask = new Set;
        for (const group of allTestGroupsInTask)
            group.requestedCommitSets().forEach((commitSet) => allCommitSetsInTask.add(commitSet));

        await fetchingMeasurementSetPromise;

        const series = measurementSet.fetchedTimeSeries('current', false, false);
        const startPoint = series.findById(this.startMeasurementId());
        const endPoint = series.findById(this.endMeasurementId());

        return Array.from(series.viewBetweenPoints(startPoint, endPoint)).map((point) => point.commitSet());
    }

    static categories()
    {
        return [
            'unconfirmed',
            'investigated',
            'closed'
        ];
    }

    static fetchById(id, noCache)
    {
        return this._fetchSubset({id: id}, noCache).then((data) => AnalysisTask.findById(id));
    }

    static fetchByBuildRequestId(id)
    {
        return this._fetchSubset({buildRequest: id}).then(function (tasks) { return tasks[0]; });
    }

    static fetchByPlatformAndMetric(platformId, metricId, noCache)
    {
        return this._fetchSubset({platform: platformId, metric: metricId}, noCache).then(function (data) {
            return AnalysisTask.findByPlatformAndMetric(platformId, metricId);
        });
    }

    static fetchRelatedTasks(taskId)
    {
        // FIXME: We should add new sever-side API to just fetch the related tasks.
        return this.fetchAll().then(function () {
            var task = AnalysisTask.findById(taskId);
            if (!task)
                return undefined;
            var relatedTasks = new Set;
            for (var bug of task.bugs()) {
                for (var otherTask of AnalysisTask.all()) {
                    if (otherTask.bugs().includes(bug))
                        relatedTasks.add(otherTask);
                }
            }
            for (var otherTask of AnalysisTask.all()) {
                if (task.isCustom())
                    continue;
                if (task.endTime() < otherTask.startTime()
                    || otherTask.endTime() < task.startTime()
                    || task.metric() != otherTask.metric())
                    continue;
                relatedTasks.add(otherTask);
            }
            return Array.from(relatedTasks);
        });
    }

    static _fetchSubset(params, noCache)
    {
        if (this._fetchAllPromise && !noCache)
            return this._fetchAllPromise;
        return this.cachedFetch('/api/analysis-tasks', params, noCache).then(this._constructAnalysisTasksFromRawData.bind(this));
    }

    static fetchAll()
    {
        if (!this._fetchAllPromise)
            this._fetchAllPromise = RemoteAPI.getJSONWithStatus('/api/analysis-tasks').then(this._constructAnalysisTasksFromRawData.bind(this));
        return this._fetchAllPromise;
    }

    static _constructAnalysisTasksFromRawData(data)
    {
        Instrumentation.startMeasuringTime('AnalysisTask', 'construction');

        // FIXME: The backend shouldn't create a separate bug row per task for the same bug number.
        var taskToBug = {};
        for (var rawData of data.bugs) {
            rawData.bugTracker = BugTracker.findById(rawData.bugTracker);
            if (!rawData.bugTracker)
                continue;

            var bug = Bug.ensureSingleton(rawData);
            if (!taskToBug[rawData.task])
                taskToBug[rawData.task] = [];
            taskToBug[rawData.task].push(bug);
        }

        for (var rawData of data.commits) {
            rawData.repository = Repository.findById(rawData.repository);
            if (!rawData.repository)
                continue;
            CommitLog.ensureSingleton(rawData.id, rawData);
        }

        function resolveCommits(commits) {
            return commits.map(function (id) { return CommitLog.findById(id); }).filter(function (commit) { return !!commit; });
        }

        var results = [];
        for (var rawData of data.analysisTasks) {
            rawData.platform = Platform.findById(rawData.platform);
            rawData.metric = Metric.findById(rawData.metric);
            rawData.bugs = taskToBug[rawData.id];
            rawData.causes = resolveCommits(rawData.causes);
            rawData.fixes = resolveCommits(rawData.fixes);
            results.push(AnalysisTask.ensureSingleton(rawData.id, rawData));
        }

        Instrumentation.endMeasuringTime('AnalysisTask', 'construction');

        return results;
    }

    static async create(name, startPoint, endPoint, testGroupName=null, repetitionCount=0, notifyOnCompletion=false)
    {
        const parameters = {name, startRun: startPoint.id, endRun: endPoint.id};
        if (testGroupName) {
            console.assert(repetitionCount);
            parameters['revisionSets'] = CommitSet.revisionSetsFromCommitSets([startPoint.commitSet(), endPoint.commitSet()]);
            parameters['repetitionCount'] = repetitionCount;
            parameters['testGroupName'] = testGroupName;
            parameters['needsNotification'] = notifyOnCompletion;
        }
        const response = await PrivilegedAPI.sendRequest('create-analysis-task', parameters);
        return AnalysisTask.fetchById(response.taskId, true);
    }
}

if (typeof module != 'undefined')
    module.exports.AnalysisTask = AnalysisTask;
