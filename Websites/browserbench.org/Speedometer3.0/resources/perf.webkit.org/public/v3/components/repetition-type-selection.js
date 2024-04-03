class RepetitionTypeSelection extends ComponentBase {
    #triggerableConfiguration;
    #selectedRepetitionType;
    #renderRepetitionTypeListLazily;
    #disabled;

    constructor()
    {
        super('repetition-type-selection');
        this.#selectedRepetitionType = null;
        this.#triggerableConfiguration = null;
        this.#disabled = false;
        this.#renderRepetitionTypeListLazily = new LazilyEvaluatedFunction(this._renderRepetitionTypeList.bind(this));
    }

    didConstructShadowTree()
    {
        const repetitionType = this.content('repetition-type');
        repetitionType.onchange = () => this.#selectedRepetitionType = repetitionType.value;
    }

    get selectedRepetitionType() { return this.#selectedRepetitionType; }
    set selectedRepetitionType(repetitionType)
    {
        console.assert(!this.#triggerableConfiguration || this.#triggerableConfiguration.isSupportedRepetitionType(repetitionType));
        this.#selectedRepetitionType = repetitionType;
        this.enqueueToRender();
    }

    set disabled(value)
    {
        console.assert(typeof value == 'boolean');
        this.#disabled = value;
        this.enqueueToRender();
    }

    setTestAndPlatform(test, platform)
    {
        this.#triggerableConfiguration = TriggerableConfiguration.findByTestAndPlatform(test, platform);
        if (!this.#triggerableConfiguration)
            this.#selectedRepetitionType = null;
        else if (!this.#triggerableConfiguration.isSupportedRepetitionType(this.#selectedRepetitionType))
            this.#selectedRepetitionType = this.#triggerableConfiguration.supportedRepetitionTypes[0];
        this.enqueueToRender();
    }

    labelForRepetitionType(repetitionType)
    {
        return {
            'alternating': 'alternating (ABAB)',
            'sequential': 'sequential (AABB)',
            'paired-parallel': 'parallel (paired A&B)',
        }[repetitionType];
    }

    render()
    {
        super.render();
        this.#renderRepetitionTypeListLazily.evaluate(this.#disabled, this.#selectedRepetitionType, ...(this.#triggerableConfiguration?.supportedRepetitionTypes || []));
    }

    _renderRepetitionTypeList(disabled, selectedRepetitionType, ...supportedRepetitionTypes)
    {
        this.renderReplace(this.content('repetition-type'), supportedRepetitionTypes.map((repetitionType) =>
            this.createElement('option', {selected: repetitionType == selectedRepetitionType, value: repetitionType},
                this.labelForRepetitionType(repetitionType))
        ));
        this.content('repetition-type').disabled = disabled;
    }

    static htmlTemplate()
    {
        return `<select id="repetition-type"></select>`;
    }
}

ComponentBase.defineElement('repetition-type-selection', RepetitionTypeSelection);