
class CreateAnalysisTaskPage extends PageWithHeading {
    constructor()
    {
        super('Create Analysis Task');
        this._message = null;
        this._renderMessageLazily = new LazilyEvaluatedFunction(this._renderMessage.bind(this));
    }

    title() { return 'Creating a New Analysis Task'; }
    routeName() { return 'analysis/task/create'; }

    updateFromSerializedState(state, isOpen)
    {
        if (state.error instanceof Set)
            state.error = Array.from(state.error.values())[0];
        this._message = state.error || (state.inProgress ? 'Creating the analysis task page...' : '');
    }

    didConstructShadowTree()
    {
        this.part('configurator').listenToAction('commitSetChange', () => this.enqueueToRender());
        this.content('start-button').onclick = this.createEventHandler(() => this._createAnalysisTaskWithGroup());
    }

    _createAnalysisTaskWithGroup()
    {
        const taskNameInput = this.content('task-name');
        if (!taskNameInput.reportValidity())
            return;

        const testGroupInput = this.content('test-group-name');
        if (!testGroupInput.reportValidity())
            return;

        const configurator = this.part('configurator');
        const tests = configurator.tests();
        if (tests.length != 1)
            return alert('Exactly one test must be selected');

        const taskName = taskNameInput.value;
        const testGroupName = testGroupInput.value;
        const iterationCount = this.content('iteration-count').value;
        const platform = configurator.platform();
        const commitSets = configurator.commitSets();

        TestGroup.createWithTask(taskName, platform, tests[0], testGroupName, iterationCount, commitSets).then((task) => {
            console.log('yay?', task);
            const url = this.router().url(`analysis/task/${task.id()}`);
            console.log('moving to ' + url);
            location.href = this.router().url(`analysis/task/${task.id()}`);
        }, (error) => {
            alert('Failed to create a new test group: ' + error);
        });
    }

    render()
    {
        super.render();
        const configurator = this.part('configurator');
        this._renderMessageLazily.evaluate(this._message, !!configurator.commitSets(), configurator.tests(), configurator.platform());
    }

    _renderMessage(message, hasValidCommitSets, tests, platform)
    {
        const messageContainer = this.content('message');
        messageContainer.textContent = this._message;
        messageContainer.style.display = this._message ? null : 'none';
        this.content('new-task').style.display = this._message ? 'none' : null;
        if (platform)
            this.content('test-group-name').value = `${tests.map((test) => test.name()).join(', ')} on ${platform.name()}`;
        this.content('iteration-start-pane').style.display = !this._message && hasValidCommitSets ? null : 'none';
      }

    static htmlTemplate()
    {
        return `
            <p id="message"></p>
            <div id="new-task">
                <input id="task-name" type="text" placeholder="Name this task" required>
                <custom-analysis-task-configurator id="configurator"></custom-analysis-task-configurator>
                <div id="iteration-start-pane">
                    <input id="test-group-name" placeholder="Name this test group" required>
                    <label><select id="iteration-count">
                        <option>1</option>
                        <option>2</option>
                        <option>3</option>
                        <option selected>4</option>
                        <option>5</option>
                        <option>6</option>
                    </select> iterations per configuration</label>
                    <button id="start-button">Start</button>
                </div>
            </div>`;
    }

    static cssTemplate()
    {
        return `
            #message {
                text-align: center;
            }

            #new-task > * {
                display: block;
                margin: 1rem;
            }

            #new-task input {
                width: 30%;
                min-width: 10rem;
                font-size: 1rem;
                font-weight: inherit;
            }

            #start-button {
                display: block;
                margin-top: 0.5rem;
                font-size: 1.2rem;
                font-weight: inherit;
            }
`;
    }
}
