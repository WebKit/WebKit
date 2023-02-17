const Constraints  = {
    Number: 'Number',
    Float: 'Float',
}

class ConstrainedVariable {
    constructor(variable, constraint) {
        this.variable = variable
        this.constraint = constraint
    }

    toString() {
        return `${this.variable} <: ${this.constraint}`
    }

    dump(out, counter) {
        out.line(`TypeVariable ${this.variable.name} { ${counter.typeId()}, TypeVariable::${this.constraint} };`)
    }
}

class TypeVariable {
    constructor(name, constraint) {
        this.name = name;
    }

    toString() {
        return this.name
    }

    dump(out, counter) {
        out.line(`TypeVariable ${this.name} { ${counter.typeId()} };`)
    }

    toCpp() {
        return this.name
    }
}

class NumericVariable {
    constructor(name) {
        this.name = name
    }

    toString() {
        return this.name
    }

    dump(out, counter) {
        out.line(`NumericVariable ${this.name} { ${counter.numericId()} };`)
    }

    toCpp() {
        return this.name
    }
}

class AbstractType {
    constructor(name, ...args) {
        this.name = name
        this.arguments = args
    }

    toString() {
        return `${this.name}<${this.arguments.join(', ')}>`
    }

    toCpp() {
        return `Abstract${this.name} { ${this.arguments.join(', ')} }`
    }
}

class FunctionType {
    constructor(variables, parameters, result) {
        this.variables = variables
        this.parameters = parameters
        this.result = result
    }

    toString() {
        return `${this.name} :: <${this.variables.join(', ')}>(${this.parameters.join(', ')}) -> ${this.result}`
    }

    dump(out) {
        const counter = (() => {
            let type = 0;
            let numeric = 0;
            return { typeId: () => type++, numericId: () => numeric++ }
        })();

        out.line('([&]() -> OverloadCandidate {')
        out.indent(() => {
            out.line(`// #{name} :: ${this.toString()}`)
            out.line('OverloadCandidate candidate;')
            this.variables.forEach(v => v.dump(out, counter))
            this.parameters.forEach(p => {
                out.line(`candidate.parameters.append(${p.toCpp()});`)
            })
            out.line(`candidate.result = ${this.result.toCpp()};`)
            out.line('return candidate;')
        })
        out.line('}()),')
    }
}


class Output {
    out = []
    indentation = 0
    indentationSize = 4

    indent(callback) {
        this.indentation++
        callback()
        this.indentation--
    }

    line(...args) {
        this.out.push(Array(this.indentation * this.indentationSize).fill(' ').join(''), ...args, '\n')
    }

    toString() {
        return this.out.join('')
    }
}

class DSL {
    static context = {}
    static declarations = {}

    static prologue() {
        // constraints
        this.context.Number = variable => new ConstrainedVariable(variable, 'Number')
        this.context.Float = variable => new ConstrainedVariable(variable, 'Float')

        // types
        this.context.Vector = (type, size) => new AbstractType('Vector', type, size)
        this.context.Matrix = (type, columns, rows) => new AbstractType('Matrix', type, columns, rows)

        // variables
        this.context.T = new TypeVariable('T')
        this.context.N = new NumericVariable('N')
        this.context.C = new NumericVariable('C')
        this.context.R = new NumericVariable('R')

        // helpers
        this.context.type = (name, variables, parameters, result) => {
            const type = new FunctionType(variables, parameters, result)
            let declarations = this.declarations[name]
            if (!declarations)
                declarations = this.declarations[name] = []
            declarations.push(type)
        }
    }

    static run(input) {
        (new Function( `with(this) { ${read(input)} }`)).call(this.context)
    }

    static dump() {
        const out = new Output();
        for (const [name, overloads] of Object.entries(this.declarations)) {
            out.line(`m_overloadedOperations.add("${name}"_s, Vector<OverloadCandidate>({`)
            out.indent(() => {
                overloads.forEach(overload => {
                    overload.dump(out)
                })
            })
            out.line('}));')
        }
        print(out.toString())
    }
}

if (typeof arguments === 'undefined' || arguments.length !== 1)
    throw new Error('usage: WGSL/generator/main.js <definition-file>')

const input = arguments[0]

DSL.prologue()
DSL.run(input)
DSL.dump()
