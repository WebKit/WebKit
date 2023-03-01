class Constraint
    attr_reader :name

    def initialize(name)
        @name = name
    end

    def to_s
        name.to_s
    end

    def self.from_type(type)
        Constraint.new type.name
    end

    def to_cpp
        "TypeVariable::#{self}"
    end
end

class VariableKind
    attr_accessor :name

    def initialize(name)
        @name = name
    end

    def to_s
        name.to_s
    end
end

class Variable
    attr_reader :name, :kind, :constraint
    def initialize(name, kind, constraint=nil)
        @name = name
        @kind = kind
        @constraint = constraint
    end

    def <(constraint)
        raise "Expected a Constraint, found: #{constraint.class} "unless constraint.class == Constraint
        Variable.new(@name, @kind, constraint)
    end

    def to_s
        constraint = " is #{@constraint.to_s}" if @constraint
        "#{@name.to_s}#{constraint}"
    end

    def to_cpp
        to_s
    end

    def declaration(context)
        index = context[kind]
        context[kind] += 1
        constraint = ", #{@constraint.to_cpp}" if @constraint
        "#{kind} #{name} { #{index}#{constraint} };"
    end

    def append
        if @kind == DSL::type_variable
            "candidate.typeVariables.append(#{name});"
        else
            "candidate.numericVariables.append(#{name});"
        end
    end
end

class NumericValue
    attr_reader :value

    def initialize(value)
        @value = value
    end

    def to_cpp
        "AbstractValue { #{value}u }"
    end
end

class AbstractType
    attr_reader :name

    def initialize(name)
        @name = name
    end

    def [](*arguments)
        ParameterizedType.new(self, arguments)
    end

    def to_s
        @name.to_s
    end
end

class PrimitiveType
    attr_reader :name

    def initialize(name)
        @name = name
    end

    def to_s
        @name.to_s
    end

    def to_cpp
        "AbstractType { m_types.#{name.downcase}Type() }"
    end
end

class ParameterizedType
    attr_reader :base, :arguments

    def initialize(base, arguments)
        @base = base
        @arguments = arguments.map { |a| normalize_argument(a) }
    end

    def normalize_argument(argument)
        return argument if argument.is_a? Variable
        return argument if argument.is_a? PrimitiveType

        raise "Expected an Integer, found a: #{argument.class}" unless argument.is_a? Integer

        NumericValue.new(argument)
    end

    def to_s
        "#{base}<#{arguments.join(", ")}>"
    end

    def to_cpp
        "Abstract#{base} { #{arguments.map(&:to_cpp).join ", "} }"
    end
end

class FunctionType
    attr_accessor :variables,  :parameters, :return_type

    def initialize(variables, parameters, return_type=nil)
        @variables = variables
        @parameters = parameters
        @return_type = return_type
    end

    def to_s
        variables = "<#{@variables.join(', ')}>" unless @variables.empty?
        "#{variables}(#{@parameters.join(', ')}) -> #{@return_type.to_s}"
    end

    def to_cpp(name)
        context = {
            DSL::type_variable => 0,
            DSL::numeric_variable => 0,
        }
        out = []
        out << "([&]() -> OverloadCandidate {"

        def append(name)
            "candidate.parameters.append(#{name});"
        end

        prefix = "    "
        out << prefix + "// #{name} :: #{to_s}"
        out << prefix + "OverloadCandidate candidate;"
        out += variables.map { |v| prefix + v.declaration(context) }
        out += variables.map { |v| prefix + v.append }
        out += parameters.map { |p| prefix + append(p.to_cpp) }
        out << prefix + "candidate.result = #{return_type.to_cpp};"
        out << prefix + "return candidate;"
        out << "}())"
        out.join "\n"
    end
end

class Array
    def call(*arguments)
        FunctionType.new(self, arguments)
    end
end


module DSL
    @context = binding()
    @operators = {}
    @TypeVariable = VariableKind.new(:TypeVariable)
    @NumericVariable = VariableKind.new(:NumericVariable)

    def self.type_variable
        @TypeVariable
    end

    def self.numeric_variable
        @NumericVariable
    end

    def self.operator(name, map)
        overloads = []
        map.each do |function, return_type|
            function.return_type = return_type
            overloads << function
        end

        if @operators[name]
            @operators[name] += overloads
        else
            @operators[name] = overloads
        end
    end

    def self.to_cpp
        out = []
        @operators.each do |name, overloads|
            out << "m_overloadedOperations.add(\"#{name}\"_s, Vector<OverloadCandidate>({"
            overloads.each { |function| out << "#{function.to_cpp(name)}," }
            out << "}));"
        end
        out << "" # xcode compilation fails if there's not newline at the end of the file
        out.join "\n"
    end

    def self.prologue
        @context.eval <<~EOS
        Vector = AbstractType.new(:Vector)
        Matrix = AbstractType.new(:Matrix)
        Array = AbstractType.new(:Array)

        Bool = PrimitiveType.new(:Bool)
        I32 = PrimitiveType.new(:I32)
        U32 = PrimitiveType.new(:U32)
        F32 = PrimitiveType.new(:F32)
        AbstractInt = PrimitiveType.new(:AbstractInt)


        T = Variable.new(:T, @TypeVariable)
        N = Variable.new(:N, @NumericVariable)
        C = Variable.new(:C, @NumericVariable)
        R = Variable.new(:R, @NumericVariable)

        Number = Constraint.new(:Number)
        Float = Constraint.new(:Float)
        EOS
    end

    def self.run(file)
        @context.eval(File.open(file).read, file)
    end

    def self.write_to(output)
        File.open(output, 'w') { |file| file.write(to_cpp) }
    end
end

raise "usage: #{__FILE__} <declaration-file> <output-file>" if ARGV.length != 2
input = ARGV[0]
output = ARGV[1]

DSL::prologue()
DSL::run(input)
DSL::write_to(output)
