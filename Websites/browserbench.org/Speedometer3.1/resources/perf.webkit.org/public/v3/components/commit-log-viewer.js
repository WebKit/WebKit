
class CommitLogViewer extends ComponentBase {

    constructor()
    {
        super('commit-log-viewer');
        this._lazilyFetchCommitLogs = new LazilyEvaluatedFunction(this._fetchCommitLogs.bind(this));
        this._repository = null;
        this._fetchingPromise = null;
        this._commits = null;
        this._precedingCommit = null;
        this._renderCommitListLazily = new LazilyEvaluatedFunction(this._renderCommitList.bind(this));
        this._showRepositoryName = true;
    }

    setShowRepositoryName(shouldShow)
    {
        this._showRepositoryName = shouldShow;
        this.enqueueToRender();
    }

    view(repository, precedingRevision, lastRevision)
    {
        this._lazilyFetchCommitLogs.evaluate(repository, precedingRevision, lastRevision);
    }

    _fetchCommitLogs(repository, precedingRevision, lastRevision)
    {
        this._fetchingPromise = null;
        this._commits = null;

        if (!repository || !lastRevision) {
            this._repository = null;
            this.enqueueToRender();
            return;
        }

        let promise;
        const fetchSingleCommit = !precedingRevision || precedingRevision == lastRevision;
        if (fetchSingleCommit) {
            promise = CommitLog.fetchForSingleRevision(repository, lastRevision).then((commits) => {
                if (this._fetchingPromise != promise)
                    return;
                this._commits = commits;
                this._precedingCommit = null;
            });
        } else {
            promise = Promise.all([
                CommitLog.fetchBetweenRevisions(repository, precedingRevision, lastRevision).then((commits) => {
                    if (this._fetchingPromise != promise)
                        return;
                    this._commits = commits;
                }),
                CommitLog.fetchForSingleRevision(repository, precedingRevision).then((precedingCommit) => {
                    if (this._fetchingPromise != promise)
                        return;
                    this._precedingCommit = precedingCommit[0];
                })
            ]);
        }

        this._repository = repository;
        this._fetchingPromise = promise;

        this._fetchingPromise.then((commits) => {
            if (this._fetchingPromise != promise)
                return;
            this._fetchingPromise = null;
            this.enqueueToRender();
        }, (error) => {
            if (this._fetchingPromise != promise)
                return;
            this._fetchingPromise = null;
            this._commits = null;
            this._precedingCommit = null;
            this.enqueueToRender();
        });

        this.enqueueToRender();
    }

    render()
    {
        const shouldShowRepositoryName = this._repository && (this._commits || this._fetchingPromise) && this._showRepositoryName;
        this.content('repository-name').textContent = shouldShowRepositoryName ? this._repository.name() : '';
        this.content('spinner-container').style.display = this._fetchingPromise ? null : 'none';
        this._renderCommitListLazily.evaluate(this._commits, this._precedingCommit);
    }

    _renderCommitList(commits, precedingCommit)
    {
        const element = ComponentBase.createElement;
        const link = ComponentBase.createLink;
        commits = commits && precedingCommit && precedingCommit.ownsCommits() ? [precedingCommit].concat(commits) : commits;
        let previousCommit = null;

        this.renderReplace(this.content('commits-list'), (commits || []).map((commit) => {
            const label = commit.label();
            const url = commit.url();
            const ownsCommits = previousCommit && previousCommit.ownsCommits() && commit.ownsCommits();
            const ownedCommitDifferenceRow = ownsCommits ? element('tr', element('td', {colspan: 2}, new OwnedCommitViewer(previousCommit, commit))) : [];
            previousCommit = commit;
            return [ownedCommitDifferenceRow,
                element('tr', [
                    element('th', [element('h4', {class: 'revision'}, url ? link(label, commit.title(), url) : label), commit.author() || '']),
                    element('td', commit.message() ? commit.message().substring(0, 80) : '')])];
        }));
    }

    static htmlTemplate()
    {
        return `
            <div class="commits-viewer-container">
                <div id="spinner-container"><spinner-icon id="spinner"></spinner-icon></div>
                <table id="commits-viewer-table">
                    <caption id="repository-name"></caption>
                    <tbody id="commits-list"></tbody>
                </table>
            </div>
`;
    }

    static cssTemplate()
    {
        return `
            .commits-viewer-container {
                width: 100%;
                height: calc(100% - 2px);
                overflow-y: scroll;
            }

            #commits-viewer-table {
                width: 100%;
                border-collapse: collapse;
                border-bottom: solid 1px #ccc;
            }

            caption {
                font-weight: inherit;
                font-size: 1rem;
                text-align: center;
                padding: 0.2rem;
            }

            .revision {
                white-space: nowrap;
                font-weight: normal;
                margin: 0;
                padding: 0;
            }

            td, th {
                word-break: break-word;
                border-top: solid 1px #ccc;
                padding: 0.2rem;
                margin: 0;
                font-size: 0.8rem;
                font-weight: normal;
            }

            #spinner-container {
                margin-top: 2rem;
                text-align: center;
            }
`;
    }

}

ComponentBase.defineElement('commit-log-viewer', CommitLogViewer);
