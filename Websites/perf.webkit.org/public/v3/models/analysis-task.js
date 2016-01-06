
class AnalysisTask extends LabeledObject {
    constructor(id, object)
    {
        super(id, object);
        this._author = object.author;
        this._createdAt = object.createdAt;

        console.assert(object.platform instanceof Platform);
        this._platform = object.platform;

        console.assert(object.metric instanceof Metric);
        this._metric = object.metric;

        this._startMeasurementId = object.startRun;
        this._startTime = object.startRunTime;
        this._endMeasurementId = object.endRun;
        this._endTime = object.endRunTime;
        this._category = object.category;
        this._changeType = object.result;
        this._needed = object.needed;
        this._bugs = object.bugs || [];
        this._buildRequestCount = object.buildRequestCount;
        this._finishedBuildRequestCount = object.finishedBuildRequestCount;
    }

    static findByPlatformAndMetric(platformId, metricId)
    {
        return this.all().filter(function (task) { return task._platform.id() == platformId && task._metric.id() == metricId; });
    }

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
    platform() { return this._platform; }
    metric() { return this._metric; }
    category() { return this._category; }
    changeType() { return this._changeType; }

    static categories()
    {
        return [
            'unconfirmed',
            'bisecting',
            'identified',
            'closed'
        ];
    }

    static fetchById(id)
    {
        return this._fetchSubset({id: id}).then(function (data) { return AnalysisTask.findById(id); });
    }

    static fetchByBuildRequestId(id)
    {
        return this._fetchSubset({buildRequest: id}).then(function (tasks) { return tasks[0]; });
    }

    static fetchByPlatformAndMetric(platformId, metricId)
    {
        return this._fetchSubset({platform: platformId, metric: metricId}).then(function (data) {
            return AnalysisTask.findByPlatformAndMetric(platformId, metricId);
        });
    }

    static _fetchSubset(params)
    {
        if (this._fetchAllPromise)
            return this._fetchAllPromise;
        return this.cachedFetch('../api/analysis-tasks', params).then(this._constructAnalysisTasksFromRawData.bind(this));
    }

    static fetchAll()
    {
        if (!this._fetchAllPromise)
            this._fetchAllPromise = getJSONWithStatus('../api/analysis-tasks').then(this._constructAnalysisTasksFromRawData.bind(this));
        return this._fetchAllPromise;
    }

    static _constructAnalysisTasksFromRawData(data)
    {
        Instrumentation.startMeasuringTime('AnalysisTask', 'construction');

        // FIXME: The backend shouldn't create a separate bug row per task for the same bug number.
        var taskToBug = {};
        for (var rawData of data.bugs) {
            var id = rawData.bugTracker + '-' + rawData.number;
            rawData.bugTracker = BugTracker.findById(rawData.bugTracker);
            if (!rawData.bugTracker)
                continue;

            var bug = Bug.findById(id) || new Bug(id, rawData);
            if (!taskToBug[rawData.task])
                taskToBug[rawData.task] = [];
            taskToBug[rawData.task].push(bug);
        }

        var results = [];
        for (var rawData of data.analysisTasks) {
            rawData.platform = Platform.findById(rawData.platform);
            rawData.metric = Metric.findById(rawData.metric);
            if (!rawData.platform || !rawData.metric)
                continue;

            rawData.bugs = taskToBug[rawData.id];
            var task = AnalysisTask.findById(rawData.id) || new AnalysisTask(rawData.id, rawData);
            results.push(task);
        }

        Instrumentation.endMeasuringTime('AnalysisTask', 'construction');

        return results;
    }

    static create(name, startRunId, endRunId)
    {
        return PrivilegedAPI.sendRequest('create-analysis-task', {
            name: name,
            startRun: startRunId,
            endRun: endRunId,
        });
    }
}
