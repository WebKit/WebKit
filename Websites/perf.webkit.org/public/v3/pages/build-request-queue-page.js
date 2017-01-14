
class BuildRequestQueuePage extends PageWithHeading {
    constructor()
    {
        super('build-request-queue-page');
        this._triggerables = [];
    }

    routeName() { return 'analysis/queue'; }
    pageTitle() { return 'Build Request Queue'; }

    open(state)
    {
        var self = this;
        BuildRequest.fetchTriggerables().then(function (list) {
            self._triggerables = list.map(function (entry) {
                var triggerable = {name: entry.name, buildRequests: BuildRequest.cachedRequestsForTriggerableID(entry.id)};

                BuildRequest.fetchForTriggerable(entry.name).then(function (requests) {
                    triggerable.buildRequests = requests;
                    self.updateRendering();
                });

                return triggerable;
            });
            self.updateRendering();
        });

        AnalysisTask.fetchAll().then(function () {
            self.updateRendering();
        });

        super.open(state);
    }

    render()
    {
        super.render();

        var referenceTime = Date.now();
        this.renderReplace(this.content().querySelector('.triggerable-list'),
            this._triggerables.map(this._constructBuildRequestTable.bind(this, referenceTime)));
    }

    _constructBuildRequestTable(referenceTime, triggerable)
    {
        if (!triggerable.buildRequests.length)
            return [];

        var rowList = [];
        var previousRow = null;
        var requestCount = 0;
        var requestCountForGroup = {};
        for (var request of triggerable.buildRequests) {
            var groupId = request.testGroupId();
            if (groupId in requestCountForGroup)
                requestCountForGroup[groupId]++;
            else
                requestCountForGroup[groupId] = 1

            if (request.hasFinished())
                continue;

            requestCount++;
            if (previousRow && previousRow.request.testGroupId() == groupId) {
                if (previousRow.contraction)
                    previousRow.count++;
                else
                    rowList.push({contraction: true, request: previousRow.request, count: 1});
            } else
                rowList.push({contraction: false, request: request, count: null});
            previousRow = rowList[rowList.length - 1];
        }

        var element = ComponentBase.createElement;
        var link = ComponentBase.createLink;
        var router = this.router();
        return element('table', {class: 'build-request-table'}, [
            element('caption', `${triggerable.name}: ${requestCount} pending requests`),
            element('thead', [
                element('td', 'Request ID'),
                element('td', 'Platform'),
                element('td', 'Test'),
                element('td', 'Analysis Task'),
                element('td', 'Group'),
                element('td', 'Order'),
                element('td', 'Status'),
                element('td', 'Waiting Time'),
            ]),
            element('tbody', rowList.map(function (entry) {
                if (entry.contraction) {
                    return element('tr', {class: 'contraction'}, [
                        element('td', {colspan: 8}, `${entry.count} additional requests`)
                    ]);
                }

                var request = entry.request;
                var taskId = request.analysisTaskId();
                var task = AnalysisTask.findById(taskId);
                return element('tr', [
                    element('td', {class: 'request-id'}, request.id()),
                    element('td', {class: 'platform'}, request.platform().name()),
                    element('td', {class: 'test'}, request.test().fullName()),
                    element('td', {class: 'task'}, !task ? taskId : link(task.name(), router.url(`analysis/task/${task.id()}`))),
                    element('td', {class: 'test-group'}, request.testGroupId()),
                    element('td', {class: 'order'}, `${request.order() + 1} of ${requestCountForGroup[request.testGroupId()]}`),
                    element('td', {class: 'status'}, request.statusLabel()),
                    element('td', {class: 'wait'}, request.waitingTime(referenceTime))]);
            }))]);
    }

    static htmlTemplate()
    {
        return `<div class="triggerable-list"></div>`;
    }

    static cssTemplate()
    {
        return `
            .triggerable-list {
                margin: 1rem;
            }

            .build-request-table {
                border-collapse: collapse;
                border: solid 0px #ccc;
                font-size: 0.9rem;
                margin-bottom: 2rem;
            }

            .build-request-table caption {
                text-align: left;
                font-size: 1.2rem;
                margin: 1rem 0 0.5rem 0;
                color: #c93;
            }

            .build-request-table td {
                padding: 0.2rem;
            }

            .build-request-table td:not(.test):not(.task) {
                white-space: nowrap;
            }

            .build-request-table .contraction {
                text-align: center;
                color: #999;
            }

            .build-request-table tr:not(.contraction) td {
                border-top: solid 1px #eee;
            }

            .build-request-table tr:last-child td {
                border-bottom: solid 1px #eee;
            }

            .build-request-table thead {
                font-weight: inherit;
                color: #c93;
            }
        `;
    }

}
