
class CustomConfigurationTestGroupForm extends TestGroupForm {

    constructor()
    {
        super('custom-configuration-test-group-form');
        this._hasTask = false;
        this._updateTestGroupNameLazily = new LazilyEvaluatedFunction(this._updateTestGroupName.bind(this));
    }

    setHasTask(hasTask)
    {
        this._hasTask = hasTask;
        this.enqueueToRender();
    }

    hasCommitSets()
    {
        return !!this.part('configurator').commitSets();
    }

    setConfigurations(test, platform, repetitionCount, commitSets)
    {
        const configurator = this.part('configurator');
        configurator.selectTests([test]);
        configurator.selectPlatform(platform);
        if (commitSets.length == 2)
            configurator.setCommitSets(commitSets[0], commitSets[1]);
        this.setRepetitionCount(repetitionCount);
        this.enqueueToRender();
    }

    startTesting()
    {
        const taskName = this.content('task-name').value;
        const testGroupName = this.content('group-name').value;
        const configurator = this.part('configurator');
        const commitSets = configurator.commitSets();
        const platform = configurator.platform();
        const test = configurator.tests()[0]; // FIXME: Add the support for specifying multiple tests.
        const repetitionType = this.part('repetition-type-selector').selectedRepetitionType;
        console.assert(!!this._hasTask === !taskName);
        if (!this._hasTask)
            this.dispatchAction('startTesting', testGroupName, this._repetitionCount, repetitionType, commitSets, platform, test, this._notifyOnCompletion, taskName);
        else
            this.dispatchAction('startTesting', testGroupName, this._repetitionCount, repetitionType, commitSets, platform, test, this._notifyOnCompletion);
    }

    didConstructShadowTree()
    {
        super.didConstructShadowTree();
        const configurator = this.part('configurator');
        configurator.listenToAction('testConfigChange', () => {
            const tests = configurator.tests();
            const platform = configurator.platform();
            if (platform && tests.length)
                this.setTestAndPlatform(tests[0], platform);

            this.enqueueToRender();
        });

        this.content('task-name').oninput = () => this.enqueueToRender();
        this.content('group-name').oninput = () => this.enqueueToRender();
    }

    render()
    {
        super.render();
        const configurator = this.part('configurator');
        this._updateTestGroupNameLazily.evaluate(configurator.tests(), configurator.platform());

        const needsTaskName = !this._hasTask && !this.content('task-name').value;
        this.content('iteration-start-pane').style.display = !!configurator.commitSets() ? null : 'none';
        this.content('task-name').style.display = this._hasTask ? 'none' : null;
        this.content('start-button').disabled = needsTaskName || !this.content('group-name').value;
    }

    _updateTestGroupName(tests, platform)
    {
        if (!platform || !tests.length)
            return;
        this.content('group-name').value = `${tests.map((test) => test.name()).join(', ')} on ${platform.label()}`;
    }

    static cssTemplate()
    {
        return super.cssTemplate() + `
            input {
                width: 30%;
                min-width: 10rem;
                font-size: 1rem;
                font-weight: inherit;
            }

            #form > * {
                margin-bottom: 1rem;
            }

            #start-button {
                display: block;
                margin-top: 0.5rem;
                font-size: 1.2rem;
                font-weight: inherit;
            }
            `;
    }

    static htmlTemplate()
    {
        return `<form id="form">
            <input id="task-name" type="text" placeholder="Name this task">
            <custom-analysis-task-configurator id="configurator"></custom-analysis-task-configurator>
            <div id="iteration-start-pane">
                <input id="group-name" type="text" placeholder="Test group name">
                ${super.formContent()}
                <button id="start-button">Start</button>
            </div>
        </form>`;
    }
}

ComponentBase.defineElement('custom-configuration-test-group-form', CustomConfigurationTestGroupForm);
