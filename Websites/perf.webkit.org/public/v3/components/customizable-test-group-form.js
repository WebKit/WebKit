
class CustomizableTestGroupForm extends TestGroupForm {

    constructor()
    {
        super('customizable-test-group-form');
        this._rootSetMap = null;
        this._disabled = true;
        this._renderedRepositorylist = null;
        this._customized = false;
        this.content().querySelector('a').onclick = this._customize.bind(this);
    }

    setRootSetMap(map)
    {
        this._rootSetMap = map;
        this._customized = false;
        this.setDisabled(!map);
    }

    _submitted()
    {
        if (this._startCallback)
            this._startCallback(this.content().querySelector('.name').value, this._repetitionCount, this._computeRootSetMap());
    }

    _customize(event)
    {
        event.preventDefault();
        this._customized = true;
        this.render();
    }

    _computeRootSetMap()
    {
        console.assert(this._rootSetMap);
        if (!this._customized)
            return this._rootSetMap;

        console.assert(this._renderedRepositorylist);
        var map = {};
        for (var label in this._rootSetMap) {
            var customRootSet = new CustomRootSet;
            for (var repository of this._renderedRepositorylist) {
                var className = CustomizableTestGroupForm._classForLabelAndRepository(label, repository);
                var revision = this.content().getElementsByClassName(className)[0].value;
                console.assert(revision);
                if (revision)
                    customRootSet.setRevisionForRepository(repository, revision);
            }
            map[label] = customRootSet;
        }
        return map;
    }

    render()
    {
        super.render();
        this.content().querySelector('.customize-link').style.display = this._disabled ? 'none' : null;

        if (!this._customized) {
            this.renderReplace(this.content().querySelector('.custom-table-container'), []);
            return;
        }
        var map = this._rootSetMap;
        console.assert(map);

        var repositorySet = new Set;
        var rootSetLabels = [];
        for (var label in map) {
            for (var repository of map[label].repositories())
                repositorySet.add(repository);
            rootSetLabels.push(label);
        }

        this._renderedRepositorylist = Repository.sortByNamePreferringOnesWithURL(Array.from(repositorySet.values()));

        var element = ComponentBase.createElement;
        this.renderReplace(this.content().querySelector('.custom-table-container'),
            element('table', {class: 'custom-table'}, [
                element('thead',
                    element('tr',
                        [element('td', 'Repository'), rootSetLabels.map(function (label) {
                            return element('td', {colspan: rootSetLabels.length + 1}, label);
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

    static _constructRevisionRadioButtons(rootSetMap, repository, rowLabel)
    {
        var className = this._classForLabelAndRepository(rowLabel, repository);
        var groupName = className + '-group';
        var element = ComponentBase.createElement;
        var revisionEditor = element('input', {class: className});

        var nodes = [];
        for (var labelToChoose in rootSetMap) {
            var commit = rootSetMap[labelToChoose].commitForRepository(repository);
            var checked = labelToChoose == rowLabel;
            var radioButton = this._createRadioButton(groupName, revisionEditor, commit, checked);
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
