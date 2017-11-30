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

    chance(chance)
    {
        return this.next % 1048576 < chance * 1048576;
    }

    number(under)
    {
        return this.next % under;
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
            elementChance: 0.5,
            classCount: 200,
            classChance: 0.3,
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
            leafClassMutationChance: 0.1,
            styleSeed: 1,
            domSeed: 2,
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

    static pseudoClassConfiguration()
    {
        return Object.assign(this.defaultConfiguration(), {
            name: 'Positional pseudo classes',
            pseudoClassChance: 0.1,
            pseudoClasses: [
                'nth-child(2n+1)',
                'nth-last-child(3n)',
                'nth-of-type(3n)',
                'nth-last-of-type(4n)',
                'first-child',
                'last-child',
                'first-of-type',
                'last-of-type',
                'only-of-type',
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
            this.pseudoClassConfiguration(),
            this.beforeAndAfterConfiguration(),
        ];
    }

    constructor(configuration)
    {
        this.configuration = configuration;

        this.baseStyle = document.createElement("style");
        this.baseStyle.textContent = `
            #testroot {
                font-size: 10px;
                line-height: 10px;
            }
            #testroot * {
                display: inline-block;
            }
            #testroot :empty {
                width:10px;
                height:10px;
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
        return `elem${ this.random.number(elementTypeCount) }`;
    }

    randomClassName()
    {
        const classCount = this.configuration.classCount;
        return `class${ this.random.number(classCount) }`;
    }

    randomClassNameFromRange(range)
    {
        const maximum = Math.round(range * this.configuration.classCount);
        return `class${ this.random.number(maximum) }`;
    }

    randomCombinator()
    {
        const combinators = this.configuration.combinators;
        return combinators[this.random.number(combinators.length)]
    }

    randomPseudoClass()
    {
        const pseudoClasses = this.configuration.pseudoClasses;
        return pseudoClasses[this.random.number(pseudoClasses.length)]
    }

    makeSimpleSelector(index, length)
    {
        const isLast = index == length - 1;
        const usePseudoClass = this.random.chance(this.configuration.pseudoClassChance) && this.configuration.pseudoClasses.length;
        const useElement = usePseudoClass || this.random.chance(this.configuration.elementChance); // :nth-of-type etc only make sense with element
        const useClass = !useElement || this.random.chance(this.configuration.classChance);
        const useBeforeOrAfter = isLast && this.random.chance(this.configuration.beforeAfterChance);
        let result = "";
        if (useElement)
            result += this.randomElementName();
        if (useClass) {
            // Use a smaller pool of class names on the left side of the selectors to create containers.
            result += "." + this.randomClassNameFromRange((index + 1) / length);
        }
        if (usePseudoClass)
            result +=  ":" + this.randomPseudoClass();
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
        let result = this.makeSimpleSelector(0, length);
        for (let i = 0; i < length; ++i) {
            const combinator = this.randomCombinator();
            if (combinator != ' ')
                result += " " + combinator;
            result += " " + this.makeSimpleSelector(i, length);
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
            declaration += " content: '\xa0';";

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
        const hasClasses = this.random.chance(0.5);
        if (hasClasses) {
            const count = this.random.number(3) + 1;
            for (let i = 0; i < count; ++i)
                element.classList.add(this.randomClassName());
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
            if (!element.firstChild && !this.random.chance(this.configuration.leafClassMutationChance))
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
            if (!element.firstChild && !this.random.chance(this.configuration.leafClassMutationChance))
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

    async runForever()
    {
        while (true) {
            this.addClasses(10);
            this.removeClasses(10);
            this.addLeafElements(10);
            this.removeLeafElements(10);

            await nextAnimationFrame();
        }
    }
}
