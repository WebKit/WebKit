class Random
{
    constructor(seed)
    {
        this.seed = seed % 2147483647;
        if (this.seed <= 0)
            this.seed += 2147483646;
    }

    get next()
    {
        return this.seed = this.seed * 16807 % 2147483647;
    }

    underOne()
    {
        return (this.next % 1048576) / 1048576;
    }

    chance(chance)
    {
        return this.underOne() < chance;
    }

    number(under)
    {
        return this.next % under;
    }

    numberSquareWeightedToLow(under)
    {
        const random = this.underOne();
        const random2 = random * random;
        return Math.floor(random2 * under);
    }
}

function nextAnimationFrame()
{
    return new Promise(resolve => requestAnimationFrame(resolve));
}

class StyleBench
{
    static defaultConfiguration()
    {
        return {
            name: 'Default',
            elementTypeCount: 10,
            idChance: 0.05,
            elementChance: 0.5,
            classCount: 200,
            classChance: 0.3,
            starChance: 0.05,
            attributeChance: 0.02,
            attributeCount: 10,
            attributeValueCount: 20,
            attributeOperators: ['','='],
            elementClassChance: 0.5,
            elementMaximumClasses: 3,
            elementAttributeChance: 0.2,
            elementMaximumAttributes: 3,
            combinators: [' ', '>',],
            pseudoClasses: [],
            pseudoClassChance: 0,
            beforeAfterChance: 0,
            maximumSelectorLength: 6,
            ruleCount: 5000,
            elementCount: 20000,
            maximumTreeDepth: 6,
            maximumTreeWidth: 50,
            repeatingSequenceChance: 0.2,
            repeatingSequenceMaximumLength: 3,
            leafMutationChance: 0.1,
            styleSeed: 1,
            domSeed: 2,
            stepCount: 5,
            mutationsPerStep: 100,
        };
    }

    static descendantCombinatorConfiguration()
    {
        return Object.assign(this.defaultConfiguration(), {
            name: 'Descendant and child combinators',
        });
    }

    static siblingCombinatorConfiguration()
    {
        return Object.assign(this.defaultConfiguration(), {
            name: 'Sibling combinators',
            combinators: [' ', ' ', '>', '>', '~', '+',],
        });
    }

    static structuralPseudoClassConfiguration()
    {
        return Object.assign(this.defaultConfiguration(), {
            name: 'Structural pseudo classes',
            pseudoClassChance: 0.1,
            pseudoClasses: [
                'first-child',
                'last-child',
                'first-of-type',
                'last-of-type',
                'only-of-type',
                'empty',
            ],
        });
    }

    static nthPseudoClassConfiguration()
    {
        return Object.assign(this.defaultConfiguration(), {
            name: 'Nth pseudo classes',
            pseudoClassChance: 0.1,
            pseudoClasses: [
                'nth-child(2n+1)',
                'nth-last-child(3n)',
                'nth-of-type(3n)',
                'nth-last-of-type(4n)',
            ],
        });
    }

    static beforeAndAfterConfiguration()
    {
        return Object.assign(this.defaultConfiguration(), {
            name: 'Before and after pseudo elements',
            beforeAfterChance: 0.1,
        });
    }

    static predefinedConfigurations()
    {
        return [
            this.descendantCombinatorConfiguration(),
            this.siblingCombinatorConfiguration(),
            this.structuralPseudoClassConfiguration(),
            this.nthPseudoClassConfiguration(),
            this.beforeAndAfterConfiguration(),
        ];
    }

    constructor(configuration)
    {
        this.configuration = configuration;
        this.idCount = 0;

        this.baseStyle = document.createElement("style");
        this.baseStyle.textContent = `
            #testroot {
                font-size: 10px;
                line-height: 10px;
            }
            #testroot * {
                display: inline-block;
                height:10px;
                min-width:10px;
            }
        `;
        document.head.appendChild(this.baseStyle);

        this.random = new Random(this.configuration.styleSeed);
        this.makeStyle();

        this.random = new Random(this.configuration.domSeed);
        this.makeTree();
    }

    randomElementName()
    {
        const elementTypeCount = this.configuration.elementTypeCount;
        return `elem${ this.random.numberSquareWeightedToLow(elementTypeCount) }`;
    }

    randomClassName()
    {
        const classCount = this.configuration.classCount;
        return `class${ this.random.numberSquareWeightedToLow(classCount) }`;
    }

    randomClassNameFromRange(range)
    {
        const maximum = Math.round(range * this.configuration.classCount);
        return `class${ this.random.numberSquareWeightedToLow(maximum) }`;
    }

    randomAttributeName()
    {
        const attributeCount = this.configuration.attributeCount;
        return `attr${ this.random.numberSquareWeightedToLow(attributeCount) }`;
    }

    randomAttributeValue()
    {
        const attributeValueCount = this.configuration.attributeValueCount;
        const valueNum = this.random.numberSquareWeightedToLow(attributeValueCount);
        if (valueNum == 0)
            return "";
        if (valueNum == 1)
            return "val";
        return `val${valueNum}`;
    }

    randomCombinator()
    {
        const combinators = this.configuration.combinators;
        return combinators[this.random.number(combinators.length)]
    }

    randomPseudoClass(isLast)
    {
        const pseudoClasses = this.configuration.pseudoClasses;
        const pseudoClass = pseudoClasses[this.random.number(pseudoClasses.length)]
        if (!isLast && pseudoClass == 'empty')
            return this.randomPseudoClass(isLast);
        return pseudoClass;
    }

    randomId()
    {
        const idCount = this.configuration.idChance * this.configuration.elementCount ;
        return `id${ this.random.number(idCount) }`;
    }

    randomAttributeSelector()
    {
        const name = this.randomAttributeName();
        const operators = this.configuration.attributeOperators;
        const operator = operators[this.random.numberSquareWeightedToLow(operators.length)];
        if (operator == '')
            return `[${name}]`;
        const value = this.randomAttributeValue();
        return `[${name}${operator}"${value}"]`;
    }

    makeCompoundSelector(index, length)
    {
        const isFirst = index == 0;
        const isLast = index == length - 1;
        const usePseudoClass = this.random.chance(this.configuration.pseudoClassChance) && this.configuration.pseudoClasses.length;
        const useId = isFirst && this.random.chance(this.configuration.idChance);
        const useElement = !useId && (usePseudoClass || this.random.chance(this.configuration.elementChance)); // :nth-of-type etc only make sense with element
        const useAttribute = !useId && this.random.chance(this.configuration.attributeChance);
        const useIdElementOrAttribute = useId || useElement || useAttribute;
        const useStar = !useIdElementOrAttribute && !isFirst && this.random.chance(this.configuration.starChance);
        const useClass = !useId && !useStar && (!useIdElementOrAttribute || this.random.chance(this.configuration.classChance));
        const useBeforeOrAfter = isLast && this.random.chance(this.configuration.beforeAfterChance);
        let result = "";
        if (useElement)
            result += this.randomElementName();
        if (useStar)
            result = "*";
        if (useId)
            result += "#" + this.randomId();
        if (useClass) {
            const classCount = this.random.numberSquareWeightedToLow(2) + 1;
            for (let i = 0; i < classCount; ++i) {
                // Use a smaller pool of class names on the left side of the selectors to create containers.
                result += "." + this.randomClassNameFromRange((index + 1) / length);
            }
        }
        if (useAttribute)
            result += this.randomAttributeSelector();

        if (usePseudoClass)
            result +=  ":" + this.randomPseudoClass(isLast);
        if (useBeforeOrAfter) {
            if (this.random.chance(0.5))
                result +=  "::before";
            else
                result +=  "::after";
        }
        return result;
    }

    makeSelector()
    {
        const length = this.random.number(this.configuration.maximumSelectorLength) + 1;
        let result = this.makeCompoundSelector(0, length);
        for (let i = 1; i < length; ++i) {
            const combinator = this.randomCombinator();
            if (combinator != ' ')
                result += " " + combinator;
            result += " " + this.makeCompoundSelector(i, length);
        }
        return result;
    }

    get randomColorComponent()
    {
        return this.random.next % 256;
    }

    makeDeclaration(selector)
    {
        let declaration = `background-color: rgb(${this.randomColorComponent}, ${this.randomColorComponent}, ${this.randomColorComponent});`;

        if (selector.endsWith('::before') || selector.endsWith('::after'))
            declaration += " content: ''; min-width:5px; display:inline-block;";

        return declaration;
    }

    makeRule()
    {
        const selector = this.makeSelector();
        return selector + " { " + this.makeDeclaration(selector) + " }";
    }

    makeStylesheet(size)
    {
        let cssText = "";
        for (let i = 0; i < size; ++i)
            cssText += this.makeRule() + "\n";
        return cssText;
    }

    makeStyle()
    {
        this.testStyle = document.createElement("style");
        this.testStyle.textContent = this.makeStylesheet(this.configuration.ruleCount);

        document.head.appendChild(this.testStyle);
    }

    makeElement()
    {
        const element = document.createElement(this.randomElementName());
        const hasClasses = this.random.chance(this.configuration.elementClassChance);
        const hasAttributes = this.random.chance(this.configuration.elementAttributeChance);
        if (hasClasses) {
            const count = this.random.numberSquareWeightedToLow(this.configuration.elementMaximumClasses) + 1;
            for (let i = 0; i < count; ++i)
                element.classList.add(this.randomClassName());
        }
        if (hasAttributes) {
            const count = this.random.number(this.configuration.elementMaximumAttributes) + 1;
            for (let i = 0; i < count; ++i)
                element.setAttribute(this.randomAttributeName(), this.randomAttributeValue());
        }
        const hasId = this.random.chance(this.configuration.idChance);
        if (hasId) {
            element.id = `id${ this.idCount }`;
            this.idCount++;
        }
        return element;
    }

    makeTreeWithDepth(parent, remainingCount, depth)
    {
        const maximumDepth = this.configuration.maximumTreeDepth;
        const maximumWidth =  this.configuration.maximumTreeWidth;
        const nonEmptyChance = (maximumDepth - depth) / maximumDepth;

        const shouldRepeat = this.random.chance(this.configuration.repeatingSequenceChance);
        const repeatingSequenceLength = shouldRepeat ? this.random.number(this.configuration.repeatingSequenceMaximumLength) + 1 : 0;

        let childCount = 0;
        if (depth == 0)
            childCount = remainingCount;
        else if (this.random.chance(nonEmptyChance))
            childCount = this.random.number(maximumWidth * depth / maximumDepth);

        let repeatingSequence = [];
        let repeatingSequenceSize = 0;
        for (let i = 0; i < childCount; ++i) {
            if (shouldRepeat && repeatingSequence.length == repeatingSequenceLength && repeatingSequenceSize < remainingCount) {
                for (const subtree of repeatingSequence)
                    parent.appendChild(subtree.cloneNode(true));
                remainingCount -= repeatingSequenceSize;
                if (!remainingCount)
                    return 0;
                continue;
            }
            const element = this.makeElement();
            parent.appendChild(element);

            if (!--remainingCount)
                return 0;
            remainingCount = this.makeTreeWithDepth(element, remainingCount, depth + 1);
            if (!remainingCount)
                return 0;

            if (shouldRepeat && repeatingSequence.length < repeatingSequenceLength) {
                repeatingSequence.push(element);
                repeatingSequenceSize += element.querySelectorAll("*").length + 1;
            }
        }
        return remainingCount;
    }

    makeTree()
    {
        this.testRoot = document.querySelector("#testroot");
        const elementCount = this.configuration.elementCount;

        this.makeTreeWithDepth(this.testRoot, elementCount, 0);

        this.updateCachedTestElements();
    }

    updateCachedTestElements()
    {
        this.testElements = this.testRoot.querySelectorAll("*");
    }

    randomTreeElement()
    {
        const randomIndex = this.random.number(this.testElements.length);
        return this.testElements[randomIndex]
    }

    addClasses(count)
    {
        for (let i = 0; i < count;) {
            const element = this.randomTreeElement();
            // There are more leaves than branches. Avoid skewing towards leaf mutations.
            if (!element.firstChild && !this.random.chance(this.configuration.leafMutationChance))
                continue;
            ++i;
            const classList = element.classList;
            classList.add(this.randomClassName());
        }
    }

    removeClasses(count)
    {
        for (let i = 0; i < count;) {
            const element = this.randomTreeElement();
            const classList = element.classList;
            if (!element.firstChild && !this.random.chance(this.configuration.leafMutationChance))
                continue;
            if (!classList.length)
                continue;
            ++i;
            classList.remove(classList[0]);
        }
    }

    addLeafElements(count)
    {
        for (let i = 0; i < count;) {
            const parent = this.randomTreeElement();
            // Avoid altering tree shape by turning many leaves into containers.
            if (!parent.firstChild)
                continue;
            ++i;
            const children = parent.childNodes;
            const index = this.random.number(children.length + 1);
            parent.insertBefore(this.makeElement(), children[index]);
        }
        this.updateCachedTestElements();
    }

    removeLeafElements(count)
    {
        for (let i = 0; i < count;) {
            const element = this.randomTreeElement();

            const canRemove = !element.firstChild && element.parentNode;
            if (!canRemove)
                continue;
            ++i;
            element.parentNode.removeChild(element);
        }
        this.updateCachedTestElements();
    }

    mutateAttributes(count)
    {
        for (let i = 0; i < count;) {
            const element = this.randomTreeElement();
            // There are more leaves than branches. Avoid skewing towards leaf mutations.
            if (!element.firstChild && !this.random.chance(this.configuration.leafMutationChance))
                continue;
            const attributeNames = element.getAttributeNames();
            let mutatedAttributes = false;
            for (const name of attributeNames) {
                if (name == "class" || name == "id")
                    continue;
                if (this.random.chance(0.5))
                    element.removeAttribute(name);
                else
                    element.setAttribute(name, this.randomAttributeValue());
                mutatedAttributes = true;
            }
            if (!mutatedAttributes) {
                const attributeCount = this.random.number(this.configuration.elementMaximumAttributes) + 1;
                for (let j = 0; j < attributeCount; ++j)
                    element.setAttribute(this.randomAttributeName(), this.randomAttributeValue());
            }
            ++i;
        }
    }

    async runForever()
    {
        while (true) {
            this.addClasses(10);
            this.removeClasses(10);
            this.addLeafElements(10);
            this.removeLeafElements(10);
            this.mutateAttributes(10);

            await nextAnimationFrame();
        }
    }
}
