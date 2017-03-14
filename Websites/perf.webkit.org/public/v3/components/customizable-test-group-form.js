
class CustomizableTestGroupForm extends TestGroupForm {

    constructor()
    {
        super('customizable-test-group-form');
        this._commitSetMap = null;
        this._renderedRepositorylist = null;
        this._customized = false;
        this._nameControl = this.content().querySelector('.name');
        this._nameControl.oninput = () => { this.enqueueToRender(); }
        this.content().querySelector('a').onclick = this._customize.bind(this);
    }

    setCommitSetMap(map)
    {
        this._commitSetMap = map;
        this._customized = false;
    }

    _submitted()
    {
        if (this._startCallback)
            this._startCallback(this._nameControl.value, this._repetitionCount, this._computeCommitSetMap());
    }

    _customize(event)
    {
        event.preventDefault();
        this._customized = true;
        this.enqueueToRender();
    }

    _computeCommitSetMap()
    {
        console.assert(this._commitSetMap);
        if (!this._customized)
            return this._commitSetMap;

        console.assert(this._renderedRepositorylist);
        var map = {};
        for (var label in this._commitSetMap) {
            var customCommitSet = new CustomCommitSet;
            for (var repository of this._renderedRepositorylist) {
                var className = CustomizableTestGroupForm._classForLabelAndRepository(label, repository);
                var revision = this.content().querySelector('.' + className).value;
                console.assert(revision);
                if (revision)
                    customCommitSet.setRevisionForRepository(repository, revision);
            }
            map[label] = customCommitSet;
        }
        return map;
    }

    render()
    {
        super.render();
        var map = this._commitSetMap;

        this.content().querySelector('button').disabled = !(map && this._nameControl.value);
        this.content().querySelector('.customize-link').style.display = !map ? 'none' : null;

        if (!this._customized) {
            this.renderReplace(this.content().querySelector('.custom-table-container'), []);
            return;
        }
        console.assert(map);

        var repositorySet = new Set;
        var commitSetLabels = [];
        for (var label in map) {
            for (var repository of map[label].repositories())
                repositorySet.add(repository);
            commitSetLabels.push(label);
        }

        this._renderedRepositorylist = Repository.sortByNamePreferringOnesWithURL(Array.from(repositorySet.values()));

        var element = ComponentBase.createElement;
        this.renderReplace(this.content().querySelector('.custom-table-container'),
            element('table', {class: 'custom-table'}, [
                element('thead',
                    element('tr',
                        [element('td', 'Repository'), commitSetLabels.map(function (label) {
                            return element('td', {colspan: commitSetLabels.length + 1}, label);
                        })])),
                element('tbody',
                    this._renderedRepositorylist.map(function (repository) {
                        var cells = [element('th', repository.label())];
                        for (var label in map)
                            cells.push(CustomizableTestGroupForm._constructRevisionRadioButtons(map, repository, label));
                        return element('tr', cells);
                    }))]));
    }

    static _classForLabelAndRepository(label, repository) { return label + '-' + repository.id(); }

    static _constructRevisionRadioButtons(commitSetMap, repository, rowLabel)
    {
        var className = this._classForLabelAndRepository(rowLabel, repository);
        var groupName = className + '-group';
        var element = ComponentBase.createElement;
        var revisionEditor = element('input', {class: className});

        const nodes = [];
        for (let labelToChoose in commitSetMap) {
            const commit = commitSetMap[labelToChoose].commitForRepository(repository);
            const checked = labelToChoose == rowLabel;
            const radioButton = this._createRadioButton(groupName, revisionEditor, commit, checked);
            if (checked)
                revisionEditor.value = commit ? commit.revision() : '';
            nodes.push(element('td', element('label', [radioButton, labelToChoose])));
        }
        nodes.push(element('td', revisionEditor));

        return nodes;
    }

    static _createRadioButton(groupName, revisionEditor, commit, checked)
    {
        var button = ComponentBase.createElement('input', {
            type: 'radio',
            name: groupName + '-radio',
            onchange: function () { revisionEditor.value = commit ? commit.revision() : ''; },
        });
        if (checked) // FIXME: createElement should be able to set boolean attribute properly.
            button.checked = true;
        return button;
    }

    static cssTemplate()
    {
        return `
            .customize-link {
                color: #333;
            }

            .customize-link a {
                color: inherit;
            }

            .custom-table {
                margin: 1rem 0;
            }

            .custom-table,
            .custom-table td,
            .custom-table th {
                font-weight: inherit;
                border-collapse: collapse;
                border-top: solid 1px #ddd;
                border-bottom: solid 1px #ddd;
                padding: 0.4rem 0.2rem;
                font-size: 0.9rem;
            }

            .custom-table thead td,
            .custom-table th {
                text-align: center;
            }
            `;
    }

    static formContent()
    {
        return `
            <input class="name" type="text" placeholder="Test group name">
            ${super.formContent()}
            <span class="customize-link">(<a href="">Customize</a>)</span>
            <div class="custom-table-container"></div>
        `;
    }
}

ComponentBase.defineElement('customizable-test-group-form', CustomizableTestGroupForm);
