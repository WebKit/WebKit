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
        "Constraints::#{self}"
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

    def to_s
      value.to_s
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
        # This is a bit hacky, but the idea is that types such as Vector can
        # either become:
        # 1) An AbstractType, if it receives a variable. This AbstractType will
        #    later be promoted to a concrete type once an overload is chosen.
        # 2) A concrete type if it's given a concrete type. Since there are no
        #    variables involved, we can immediately construct a concrete type.
        if arguments.any? do |arg| arg.is_a?(Variable) && !arg.kind.nil? end
            ParameterizedAbstractType.new(self, arguments)
        else
            Constructor.new(@name.downcase)[*arguments]
        end
    end

    def to_s
        @name.to_s
    end
end

class Constructor
    attr_reader :name, :arguments

    def initialize(name, arguments = nil)
        @name = name
        @arguments = arguments
    end

    def [](*arguments)
        return Constructor.new(@name, arguments)
    end

    def to_s
        "#{name.to_s}[#{arguments.map(&:to_s).join(", ")}]"
    end

    def to_cpp
        "AbstractType { m_types.#{name}Type(#{arguments.map { |a| a.respond_to? :to_cpp and a.to_cpp or a}.join ", "}) }"
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
        "m_types.#{name[0].downcase}#{name[1..]}Type()"
    end
end

class ParameterizedAbstractType
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
        Texture = AbstractType.new(:Texture)

        Texture1d = Variable.new(:"Types::Texture::Kind::Texture1d", nil)
        Texture2d = Variable.new(:"Types::Texture::Kind::Texture2d", nil)
        TextureMultisampled2d = Variable.new(:"Types::Texture::Kind::TextureMultisampled2d", nil)
        Texture2dArray = Variable.new(:"Types::Texture::Kind::Texture2dArray", nil)
        Texture3d = Variable.new(:"Types::Texture::Kind::Texture3d", nil)
        TextureCube = Variable.new(:"Types::Texture::Kind::TextureCube", nil)
        TextureCubeArray = Variable.new(:"Types::Texture::Kind::TextureCubeArray", nil)

        Bool = PrimitiveType.new(:Bool)
        I32 = PrimitiveType.new(:I32)
        U32 = PrimitiveType.new(:U32)
        F32 = PrimitiveType.new(:F32)
        Sampler = PrimitiveType.new(:Sampler)
        TextureExternal = PrimitiveType.new(:TextureExternal)
        AbstractInt = PrimitiveType.new(:AbstractInt)
        AbstractFloat = PrimitiveType.new(:AbstractFloat)


        S = Variable.new(:S, @TypeVariable)
        T = Variable.new(:T, @TypeVariable)
        U = Variable.new(:U, @TypeVariable)
        V = Variable.new(:V, @TypeVariable)
        N = Variable.new(:N, @NumericVariable)
        C = Variable.new(:C, @NumericVariable)
        R = Variable.new(:R, @NumericVariable)
        K = Variable.new(:K, @NumericVariable)

        Number = Constraint.new(:Number)
        Integer = Constraint.new(:Integer)
        Float = Constraint.new(:Float)
        Scalar = Constraint.new(:Scalar)
        ConcreteInteger = Constraint.new(:ConcreteInteger)
        ConcreteFloat = Constraint.new(:ConcreteFloat)
        ConcreteScalar = Constraint.new(:ConcreteScalar)
        Concrete32BitNumber = Constraint.new(:Concrete32BitNumber)
        SignedNumber = Constraint.new(:SignedNumber)
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
