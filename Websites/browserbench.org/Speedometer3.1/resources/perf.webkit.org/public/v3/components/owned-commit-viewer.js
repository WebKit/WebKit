class OwnedCommitViewer extends ComponentBase {

    constructor(previousCommit, currentCommit)
    {
        super('owned-commit-viewer');
        this._previousCommit = previousCommit;
        this._currentCommit = currentCommit;
        this._previousOwnedCommits = null;
        this._currentOwnedCommits = null;
        this._showingOwnedCommits = false;
        this._renderOwnedCommitTableLazily = new LazilyEvaluatedFunction(this._renderOwnedCommitTable.bind(this));
    }

    didConstructShadowTree()
    {
        this.part('expand-collapse').listenToAction('toggle', (expanded) => this._toggleVisibility(expanded));
    }

    _toggleVisibility(expanded)
    {
        this._showingOwnedCommits = expanded;
        this.enqueueToRender();

        Promise.all([this._previousCommit.fetchOwnedCommits(), this._currentCommit.fetchOwnedCommits()]).then((ownedCommitsList) => {
            this._previousOwnedCommits = ownedCommitsList[0];
            this._currentOwnedCommits = ownedCommitsList[1];
            this.enqueueToRender();
        });
    }

    render()
    {
        const hideSpinner = (this._previousOwnedCommits && this._currentOwnedCommits) || !this._showingOwnedCommits;

        this.content('difference-entries').style.display =  this._showingOwnedCommits ? null : 'none';
        this.content('spinner-container').style.display = hideSpinner ? 'none' : null;
        this.content('difference-table').style.display = this._showingOwnedCommits ? null : 'none';
        this._renderOwnedCommitTableLazily.evaluate(this._previousOwnedCommits, this._currentOwnedCommits);
    }

    _renderOwnedCommitTable(previousOwnedCommits, currentOwnedCommits)
    {
        if (!previousOwnedCommits || !currentOwnedCommits)
            return;

        const difference = CommitLog.ownedCommitDifferenceForOwnerCommits(this._previousCommit, this._currentCommit);
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

ComponentBase.defineElement('owned-commit-viewer', OwnedCommitViewer);
