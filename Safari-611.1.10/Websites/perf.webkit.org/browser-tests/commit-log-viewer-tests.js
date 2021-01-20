describe('CommitLogViewer', () => {

    function importCommitLogViewer(context)
    {
        const scripts = [
            '../shared/statistics.js',
            'instrumentation.js',
            'lazily-evaluated-function.js',
            'models/data-model.js',
            'models/repository.js',
            'models/commit-set.js',
            'models/commit-log.js',
            '../shared/common-component-base.js',
            'components/base.js',
            'components/spinner-icon.js',
            'components/commit-log-viewer.js'];
        return context.importScripts(scripts, 'CommonComponentBase', 'ComponentBase', 'CommitLogViewer', 'Repository', 'CommitLog', 'RemoteAPI').then(() => {
            return context.symbols.CommitLogViewer;
        });
    }

    const webkitCommit210948 = {
        "id": "185326",
        "revision": "210948",
        "repository": 1,
        "previousCommit": null,
        "ownsCommits": false,
        "time": +new Date("2017-01-20 02:52:34.577Z"),
        "authorName": "Zalan Bujtas",
        "authorEmail": "zalan@apple.com",
        "message": "a message",
    };

    const webkitCommit210949 = {
        "id": "185334",
        "revision": "210949",
        "repository": 1,
        "previousCommit": null,
        "ownsCommits": false,
        "time": +new Date("2017-01-20T03:23:50.645Z"),
        "authorName": "Chris Dumez",
        "authorEmail": "cdumez@apple.com",
        "message": "some message",
    };

    const webkitCommit210950 = {
        "id": "185338",
        "revision": "210950",
        "repository": 1,
        "previousCommit": null,
        "ownsCommits": false,
        "time": +new Date("2017-01-20T03:49:37.887Z"),
        "authorName": "Commit Queue",
        "authorEmail": "commit-queue@webkit.org",
        "message": "another message",
    };

    it('should initially be empty with no spinner', () => {
        const context = new BrowsingContext();
        return importCommitLogViewer(context).then((CommitLogViewer) => {
            const viewer = new CommitLogViewer;
            context.document.body.appendChild(viewer.element());
            viewer.enqueueToRender();
            return waitForComponentsToRender(context).then(() => {
                expect(viewer.content('spinner-container').offsetHeight).to.be(0);
                expect(viewer.content('commits-list').matches(':empty')).to.be(true);
                expect(viewer.content('repository-name').matches(':empty')).to.be(true);
            });
        });
    });

    it('should show the repository name and the spinner once it started fetching the list of commits ', () => {
        const context = new BrowsingContext();
        return importCommitLogViewer(context).then((CommitLogViewer) => {
            const Repository = context.symbols.Repository;
            const SpinnerIcon = context.symbols.SpinnerIcon;
            const ComponentBase = context.symbols.ComponentBase;
            const RemoteAPI = context.symbols.RemoteAPI;
            const viewer = new CommitLogViewer;
            const webkit = new Repository(1, {name: 'WebKit'});
            context.document.body.appendChild(viewer.element());
            viewer.enqueueToRender();
            return waitForComponentsToRender(context).then(() => {
                expect(viewer.content('spinner-container').offsetHeight).to.be(0);
                expect(viewer.content('commits-list').matches(':empty')).to.be(true);
                expect(viewer.content('repository-name').matches(':empty')).to.be(true);
                expect(RemoteAPI.requests.length, 0);
                viewer.view(webkit, '210948', '210950');
                expect(RemoteAPI.requests.length, 1);
                return waitForComponentsToRender(context);
            }).then(() => {
                expect(viewer.content('spinner-container').offsetHeight).to.not.be(0);
                expect(viewer.content('commits-list').matches(':empty')).to.be(true);
                expect(viewer.content('repository-name').matches(':empty')).to.be(false);
                expect(viewer.content('repository-name').textContent).to.contain('WebKit');
            });
        });
    });

    it('should show the repository name, the list of commits, and hide the spinner once the list of commits are fetched', () => {
        const context = new BrowsingContext();
        return importCommitLogViewer(context).then(async (CommitLogViewer) => {
            const Repository = context.symbols.Repository;
            const SpinnerIcon = context.symbols.SpinnerIcon;
            const ComponentBase = context.symbols.ComponentBase;
            const requests = context.symbols.RemoteAPI.requests;
            const viewer = new CommitLogViewer;
            const webkit = new Repository(1, {name: 'WebKit'});
            context.document.body.appendChild(viewer.element());

            viewer.enqueueToRender();
            await waitForComponentsToRender(context);

            viewer.view(webkit, '210948', '210950');
            await waitForComponentsToRender(context);

            expect(viewer.content('spinner-container').offsetHeight).to.not.be(0);
            expect(viewer.content('commits-list').matches(':empty')).to.be(true);
            expect(viewer.content('repository-name').matches(':empty')).to.be(false);
            expect(viewer.content('repository-name').textContent).to.contain('WebKit');
            expect(requests.length).to.be(2);
            expect(requests[0].url).to.be('/api/commits/1/?precedingRevision=210948&lastRevision=210950');
            expect(requests[1].url).to.be('/api/commits/1/210948');
            requests[0].resolve({
                "status": "OK",
                "commits": [webkitCommit210949, webkitCommit210950],
            });
            requests[1].resolve({
                "status": "OK",
                "commits": [webkitCommit210948],
            });

            await waitForComponentsToRender(context);

            expect(viewer.content('spinner-container').offsetHeight).to.be(0);
            expect(viewer.content('commits-list').matches(':empty')).to.be(false);
            expect(viewer.content('commits-list').textContent).to.contain('r210949');
            expect(viewer.content('commits-list').textContent).to.contain('Chris Dumez');
            expect(viewer.content('commits-list').textContent).to.contain('r210950');
            expect(viewer.content('commits-list').textContent).to.contain('Commit Queue');
            expect(viewer.content('repository-name').matches(':empty')).to.be(false);
            expect(viewer.content('repository-name').textContent).to.contain('WebKit');
            expect(viewer.content('commits-list').querySelector('a')).to.be(null);
        });
    });

});
