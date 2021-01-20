const Statistics = require('../../public/shared/statistics.js');

class MeasurementSetAnalyzer {
    constructor(measurementSetList, startTime, endTime, logger)
    {
        this._measurementSetList = measurementSetList;
        this._startTime = startTime;
        this._endTime = endTime;
        this._logger = logger;
    }

    async analyzeOnce()
    {
        for (const measurementSet of this._measurementSetList)
            await this._analyzeMeasurementSet(measurementSet);
    }

    // FIXME: This code should be shared with DashboardPage.
    static measurementSetListForAnalysis(manifest)
    {
        const measurementSetList = [];
        for (const dashboard of Object.values(manifest.dashboards)) {
            for (const row of dashboard) {
                for (const cell of row) {
                    if (cell instanceof Array) {
                        if (cell.length < 2)
                            continue;
                        const platformId = parseInt(cell[0]);
                        const metricId = parseInt(cell[1]);
                        if (isNaN(platformId) || isNaN(metricId))
                            continue;
                        const platform = Platform.findById(platformId);
                        const metric = Metric.findById(metricId);
                        console.assert(platform);
                        console.assert(metric);

                        const measurementSet = MeasurementSet.findSet(platform.id(), metric.id(), platform.lastModified(metric));
                        console.assert(measurementSet);
                        measurementSetList.push(measurementSet);
                    }
                }
            }
        }
        return measurementSetList;
    }

    async _analyzeMeasurementSet(measurementSet)
    {
        const metric = Metric.findById(measurementSet.metricId());
        const platform = Platform.findById(measurementSet.platformId());
        this._logger.info(`==== "${metric.fullName()}" on "${platform.name()}" ====`);
        try {
            await measurementSet.fetchBetween(this._startTime, this._endTime);
        } catch (error) {
            if (error != 'ConfigurationNotFound')
                throw error;
            this._logger.warn(`Skipping analysis for "${metric.fullName()}" on "${platform.name()}" as time series does not exit.`);
            return;
        }
        const currentTimeSeries = measurementSet.fetchedTimeSeries('current', false, false);
        const rawValues = currentTimeSeries.values();
        if (!rawValues || rawValues.length < 2)
            return;

        const segmentedValues = await measurementSet.fetchSegmentation('segmentTimeSeriesByMaximizingSchwarzCriterion', [], 'current', false);

        const progressionString = 'progression';
        const regressionString = 'regression';
        const ranges = Statistics.findRangesForChangeDetectionsWithWelchsTTest(rawValues, segmentedValues).map((range) =>({
            startPoint: currentTimeSeries.findPointByIndex(range.startIndex),
            endPoint: currentTimeSeries.findPointByIndex(range.endIndex),
            valueChangeSummary: metric.labelForDifference(range.segmentationStartValue, range.segmentationEndValue,
                progressionString, regressionString)
        }));

        const analysisTasks = await AnalysisTask.fetchByPlatformAndMetric(platform.id(), metric.id());
        const filteredRanges = ranges.filter((range) => {
            const rangeEndsBeforeAnalysisStarts = range.endPoint.time < this._startTime;
            if (rangeEndsBeforeAnalysisStarts)
                return false;
            for (const task of analysisTasks) {
                const taskEndsBeforeRangeStart = task.endTime() < range.startPoint.time;
                const taskStartsAfterRangeEnd = range.endPoint.time < task.startTime();
                if (!(taskEndsBeforeRangeStart || taskStartsAfterRangeEnd))
                    return false;
            }
            return true;
        });

        let rangeWithMostSignificantChange = null;
        let largestWeightFavoringRegression = 0;
        for (const range of filteredRanges) {
            const relativeChangeAbsoluteValue = Math.abs(range.valueChangeSummary.relativeChange);
            const weightFavoringRegression = range.valueChangeSummary.changeType === regressionString ?
                relativeChangeAbsoluteValue : Math.sqrt(relativeChangeAbsoluteValue);

            if (weightFavoringRegression > largestWeightFavoringRegression) {
                largestWeightFavoringRegression = weightFavoringRegression;
                rangeWithMostSignificantChange = range;
            }
        }

        if (!rangeWithMostSignificantChange) {
            this._logger.info('Nothing to analyze');
            return;
        }

        const startCommitSet = rangeWithMostSignificantChange.startPoint.commitSet();
        const endCommitSet = rangeWithMostSignificantChange.endPoint.commitSet();
        const summary = `Potential ${rangeWithMostSignificantChange.valueChangeSummary.changeLabel} on ${platform.name()} between ${CommitSet.diff(startCommitSet, endCommitSet)}`;
        const confirmingTaskArguments = Triggerable.findByTestConfiguration(metric.test(), platform) ? ['Confirm', 4, true] : [];

        // FIXME: The iteration count should be smarter than hard-coding.
        const analysisTask = await AnalysisTask.create(summary, rangeWithMostSignificantChange.startPoint,
            rangeWithMostSignificantChange.endPoint, ...confirmingTaskArguments);

        this._logger.info(`Created analysis task with id "${analysisTask.id()}" to confirm: "${summary}".`);
    }
}

if (typeof module !== 'undefined')
    module.exports.MeasurementSetAnalyzer = MeasurementSetAnalyzer;