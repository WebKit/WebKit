
class CommitLogViewer extends ComponentBase {

    constructor()
    {
        super('commit-log-viewer');
        this._repository = null;
        this._fetchingPromise = null;
        this._commits = null;
    }

    currentRepository() { return this._repository; }

    view(repository, from, to)
    {
        this._commits = null;

        if (!repository) {
            this._fetchingPromise = null;
            this._repository = null;
            return Promise.resolve(null);
        }
        
        if (!to) {
            this._fetchingPromise = null;
            return Promise.resolve(null);
        }

        var promise = CommitLog.fetchBetweenRevisions(repository, from || to, to);

        this._fetchingPromise = promise;

        var self = this;
        var spinnerTimer = setTimeout(function () {
            self.render();
        }, 300);

        this._fetchingPromise.then(function (commits) {
            clearTimeout(spinnerTimer);
            if (self._fetchingPromise != promise)
                return;
            self._repository = repository;
            self._fetchingPromise = null;
            self._commits = commits;
        });

        return this._fetchingPromise;
    }

    render()
    {
        if (this._repository)
            this.content().querySelector('caption').textContent = this._repository.name();

        var element = ComponentBase.createElement;
        var link = ComponentBase.createLink;

        this.renderReplace(this.content().querySelector('tbody'), (this._commits || []).map(function (commit) {
            var label = commit.label();
            var url = commit.url();
            return element('tr', [
                element('th', [element('h4', {class: 'revision'}, url ? link(label, commit.title(), url) : label), commit.author() || '']),
                element('td', commit.message() ? commit.message().substring(0, 80) : '')]);
        }));

        this.content().querySelector('.commits-viewer-spinner').style.display = this._fetchingPromise ? null : 'none';
    }

    static htmlTemplate()
    {
        return `
            <div class="commits-viewer-container">
                <div class="commits-viewer-spinner"><spinner-icon></spinner-icon></div>
                <table class="commits-viewer-table">
                    <caption></caption>
                    <tbody>
                    </tbody>
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
            
            .commits-viewer-table {
                width: 100%;
            }

            .commits-viewer-table caption {
                font-weight: inherit;
                font-size: 1rem;
                text-align: center;
                padding: 0.2rem;
            }

            .commits-viewer-table {
                border-collapse: collapse;
                border-bottom: solid 1px #ccc;
            }

            .commits-viewer-table .revision {
                white-space: nowrap;
                font-weight: normal;
                margin: 0;
                padding: 0;
            }

            .commits-viewer-table td,
            .commits-viewer-table th {
                word-break: break-word;
                border-top: solid 1px #ccc;
                padding: 0.2rem;
                margin: 0;
                font-size: 0.8rem;
                font-weight: normal;
            }

            .commits-viewer-spinner {
                margin-top: 2rem;
                text-align: center;
            }
`;
    }

}

ComponentBase.defineElement('commit-log-viewer', CommitLogViewer);
