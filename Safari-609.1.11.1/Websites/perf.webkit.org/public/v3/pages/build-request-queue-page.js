
class BuildRequestQueuePage extends PageWithHeading {
    constructor()
    {
        super('build-request-queue-page');
        this._buildRequestsByTriggerable = new Map;
    }

    routeName() { return 'analysis/queue'; }
    pageTitle() { return 'Build Request Queue'; }

    open(state)
    {
        for (let triggerable of Triggerable.all()) {
            BuildRequest.fetchForTriggerable(triggerable.name()).then((requests) => {
                this._buildRequestsByTriggerable.set(triggerable, requests);
                this.enqueueToRender();
            });
        }

        AnalysisTask.fetchAll().then(() => this.enqueueToRender());

        super.open(state);
    }

    render()
    {
        super.render();

        const referenceTime = Date.now();
        this.renderReplace(this.content().querySelector('.triggerable-list'),
            Triggerable.sortByName(Triggerable.all()).map((triggerable) => {
                const buildRequests = this._buildRequestsByTriggerable.get(triggerable) || [];
                return this._constructBuildRequestTable(referenceTime, triggerable, buildRequests);
            }));
    }

    _constructBuildRequestTable(referenceTime, triggerable, buildRequests)
    {
        if (!buildRequests.length)
            return [];

        const rowList = [];
        const requestCountForGroup = {};
        const requestsByGroup = {};
        let previousRow = null;
        let requestCount = 0;
        for (const request of buildRequests) {
            const groupId = request.testGroupId();
            if (groupId in requestCountForGroup) {
                requestCountForGroup[groupId]++;
                requestsByGroup[groupId].push(request);
            } else {
                requestCountForGroup[groupId] = 1;
                requestsByGroup[groupId] = [request];
            }

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

        const element = ComponentBase.createElement;
        const link = ComponentBase.createLink;
        const router = this.router();
        return element('table', {class: 'build-request-table'}, [
            element('caption', `${triggerable.name()}: ${requestCount} pending requests`),
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
            element('tbody', rowList.map((entry) => {
                if (entry.contraction) {
                    return element('tr', {class: 'contraction'}, [
                        element('td', {colspan: 8}, `${entry.count} additional requests`)
                    ]);
                }

                const request = entry.request;
                const taskId = request.analysisTaskId();
                const task = AnalysisTask.findById(taskId);
                const requestsForGroup = requestsByGroup[request.testGroupId()];
                const firstOrder = requestsForGroup[0].order();
                let testName = null;
                if (request.test())
                    testName = request.test().fullName();
                else {
                    const firstRequestToTest = requestsForGroup.find((request) => !!request.test());
                    testName = `Building (for ${firstRequestToTest.test().fullName()})`;
                }
                return element('tr', [
                    element('td', {class: 'request-id'}, request.id()),
                    element('td', {class: 'platform'}, request.platform().name()),
                    element('td', {class: 'test'}, testName),
                    element('td', {class: 'task'}, !task ? taskId : link(task.name(), router.url(`analysis/task/${task.id()}`))),
                    element('td', {class: 'test-group'}, request.testGroupId()),
                    element('td', {class: 'order'}, `${request.order() - firstOrder + 1} of ${requestCountForGroup[request.testGroupId()]}`),
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
