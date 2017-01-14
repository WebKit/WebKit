
class CreateAnalysisTaskPage extends PageWithHeading {
    constructor()
    {
        super('Create Analysis Task');
        this._errorMessage = null;
    }

    title() { return 'Creating a New Analysis Task'; }
    routeName() { return 'analysis/task/create'; }

    updateFromSerializedState(state, isOpen)
    {
        if (state.error instanceof Set)
            state.error = Array.from(state.error.values())[0];

        this._errorMessage = state.error;
        if (!isOpen)
            this.updateRendering();
    }

    render()
    {
        super.render();
        if (this._errorMessage)
            this.content().querySelector('.message').textContent = this._errorMessage;
    }

    static htmlTemplate()
    {
        return `
            <div class="create-analysis-task-container">
                <p class="message">Creating the analysis task page...</p>
            </div>
`;
    }

    static cssTemplate()
    {
        return `
            .create-analysis-task-container {
                display: flex;
            }

            .create-analysis-task-container > * {
                margin: 1rem auto;
                display: inline-block;
            }

            .create-analysis-task input {
                font-size: inherit;
            }
`;
    }
}
