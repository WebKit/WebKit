
class CustomizableTestGroupForm extends TestGroupForm {

    constructor()
    {
        super('customizable-test-group-form');
        this._commitSetMap = null;
        this._name = null;
        this._isCustomized = false;
        this._revisionEditorMap = {};

        this._renderCustomRevisionTableLazily = new LazilyEvaluatedFunction(this._renderCustomRevisionTable.bind(this));
    }

    setCommitSetMap(map)
    {
        this._commitSetMap = map;
        this._isCustomized = false;
        this.enqueueToRender();
    }

    startTesting()
    {
        this.dispatchAction('startTesting', this._repetitionCount, this._name, this._computeCommitSetMap());
    }

    didConstructShadowTree()
    {
        super.didConstructShadowTree();

        const nameControl = this.content('name');
        nameControl.oninput = () => {
            this._name = nameControl.value;
            this.enqueueToRender();
        }

        this.content('customize-link').onclick = this.createEventHandler(() => {
            this._isCustomized = true;
            this.enqueueToRender();
        });
    }

    _computeCommitSetMap()
    {
        console.assert(this._commitSetMap);
        if (!this._isCustomized)
            return this._commitSetMap;

        const map = {};
        for (const label in this._commitSetMap) {
            const originalCommitSet = this._commitSetMap;
            const customCommitSet = new CustomCommitSet;
            for (let repository of this._commitSetMap[label].repositories()) {
                const revisionEditor = this._revisionEditorMap[label].get(repository);
                console.assert(revisionEditor);
                customCommitSet.setRevisionForRepository(repository, revisionEditor.value);
            }
            map[label] = customCommitSet;
        }
        return map;
    }

    render()
    {
        super.render();

        this.content('start-button').disabled = !(this._commitSetMap && this._name);
        this.content('customize-link-container').style.display = !this._commitSetMap ? 'none' : null;

        this._renderCustomRevisionTableLazily.evaluate(this._commitSetMap, this._isCustomized);
    }

    _renderCustomRevisionTable(commitSetMap, isCustomized)
    {
        if (!commitSetMap || !isCustomized) {
            this.renderReplace(this.content('custom-table'), []);
            return null;
        }

        const repositorySet = new Set;
        const commitSetLabels = [];
        this._revisionEditorMap = {};
        for (const label in commitSetMap) {
            for (const repository of commitSetMap[label].repositories())
                repositorySet.add(repository);
            commitSetLabels.push(label);
            this._revisionEditorMap[label] = new Map;
        }

        const repositoryList = Repository.sortByNamePreferringOnesWithURL(Array.from(repositorySet.values()));
        const element = ComponentBase.createElement;
        this.renderReplace(this.content('custom-table'), [
            element('thead',
                element('tr',
                    [element('td', 'Repository'), commitSetLabels.map((label) => element('td', {colspan: commitSetLabels.length + 1}, label))])),
            element('tbody',
                repositoryList.map((repository) => {
                    const cells = [element('th', repository.label())];
                    for (const label in commitSetMap)
                        cells.push(this._constructRevisionRadioButtons(commitSetMap, repository, label));
                    return element('tr', cells);
                }))]);

        return repositoryList;
    }

    _constructRevisionRadioButtons(commitSetMap, repository, rowLabel)
    {
        const element = ComponentBase.createElement;
        const revisionEditor = element('input');

        this._revisionEditorMap[rowLabel].set(repository, revisionEditor);

        const nodes = [];
        for (let labelToChoose in commitSetMap) {
            const commit = commitSetMap[labelToChoose].commitForRepository(repository);
            const checked = labelToChoose == rowLabel;
            const radioButton = element('input', {type: 'radio', name: `${rowLabel}-${repository.id()}-radio`, checked,
                onchange: () => { revisionEditor.value = commit ? commit.revision() : ''; }});

            if (checked)
                revisionEditor.value = commit ? commit.revision() : '';
            nodes.push(element('td', element('label', [radioButton, labelToChoose])));
        }
        nodes.push(element('td', revisionEditor));

        return nodes;
    }

    static cssTemplate()
    {
        return `
            #customize-link-container,
            #customize-link {
                color: #333;
            }

            #custom-table:not(:empty) {
                margin: 1rem 0;
            }

            #custom-table,
            #custom-table td,
            #custom-table th {
                font-weight: inherit;
                border-collapse: collapse;
                border-top: solid 1px #ddd;
                border-bottom: solid 1px #ddd;
                padding: 0.4rem 0.2rem;
                font-size: 0.9rem;
            }

            #custom-table thead td,
            #custom-table th {
                text-align: center;
            }
            `;
    }

    static formContent()
    {
        return `
            <input id="name" type="text" placeholder="Test group name">
            ${super.formContent()}
            <span id="customize-link-container">(<a id="customize-link" href="#">Customize</a>)</span>
            <table id="custom-table"></table>
        `;
    }
}

ComponentBase.defineElement('customizable-test-group-form', CustomizableTestGroupForm);
