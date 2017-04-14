class SubCommitViewer extends ComponentBase {

    constructor(previousCommit, currentCommit)
    {
        super('sub-commit-viewer');
        this._previousCommit = previousCommit;
        this._currentCommit = currentCommit;
        this._previousSubCommits = null;
        this._currentSubCommits = null;
        this._showingSubCommits = false;
        this._renderSubCommitTableLazily = new LazilyEvaluatedFunction(this._renderSubcommitTable.bind(this));
    }

    didConstructShadowTree()
    {
        this.part('expand-collapse').listenToAction('toggle', (expanded) => this._toggleVisibility(expanded));
    }

    _toggleVisibility(expanded)
    {
        this._showingSubCommits = expanded;
        this.enqueueToRender();

        Promise.all([this._previousCommit.fetchSubCommits(), this._currentCommit.fetchSubCommits()]).then((subCommitsList) => {
            this._previousSubCommits = subCommitsList[0];
            this._currentSubCommits = subCommitsList[1];
            this.enqueueToRender();
        });
    }

    render()
    {
        const hideSpinner = (this._previousSubCommits && this._currentSubCommits) || !this._showingSubCommits;

        this.content('difference-entries').style.display =  this._showingSubCommits ? null : 'none';
        this.content('spinner-container').style.display = hideSpinner ? 'none' : null;
        this.content('difference-table').style.display = this._showingSubCommits ? null : 'none';
        this._renderSubCommitTableLazily.evaluate(this._previousSubCommits, this._currentSubCommits);
    }

    _renderSubcommitTable(previousSubCommits, currentSubCommits)
    {
        if (!previousSubCommits || !currentSubCommits)
            return;

        const difference = CommitLog.diffSubCommits(this._previousCommit, this._currentCommit);
        const sortedRepositories = Repository.sortByName([...difference.keys()]);
        const element = ComponentBase.createElement;

        const tableEntries = sortedRepositories.map((repository) => {
            const revisions = difference.get(repository);
            return element('tr', [element('td', repository.name()),
                element('td', revisions[0] ? revisions[0].revision() : ''),
                element('td', revisions[1] ? revisions[1].revision() : '')]);
        });
        this.renderReplace(this.content('difference-entries'), tableEntries);
    }

    static htmlTemplate()
    {
        return `
            <expand-collapse-button id="expand-collapse"></expand-collapse-button>
            <table id="difference-table">
                <tbody id="difference-entries"></tbody>
            </table>
            <div id="spinner-container"><spinner-icon id="spinner"></spinner-icon></div>`;
    }

    static cssTemplate() {
        return `
            :host {
                display: block;
                font-size: 0.8rem;
                font-weight: normal;
            }

            expand-collapse-button {
                margin-left: calc(50% - 0.8rem);
                display: block;
            }

            td, th {
                padding: 0.2rem;
                margin: 0;
                border-top: solid 1px #ccc;
            }

            #difference-table {
                width: 100%;
            }

            #spinner-container {
                text-align: center;
            }`;
    }
}

ComponentBase.defineElement('sub-commit-viewer', SubCommitViewer);