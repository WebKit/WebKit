require 'json'

module Wasm
    def self.normalize(name)
        name.gsub(/(\.|\/)/, "_")
    end

    def self.autogenerate_opcodes(context, wasm_json)
        JSON.parse(wasm_json)["opcode"].each do |name, value|
            category = value["category"]

            next unless ["arithmetic", "comparison", "conversion"].include? category

            returnCount = value["return"].size
            parameterCount = value["parameter"].size

            assert("should return 0 or 1 values") { [0, 1].include? returnCount }
            assert("should only have 1 or 2 parameters") { [1, 2].include? parameterCount }

            name = normalize(name)
            arguments = {}

            virtualRegister = context.eval("VirtualRegister")

            if returnCount > 0
                arguments[:dst] = virtualRegister
            end

            case parameterCount
            when 1
                arguments[:operand] = virtualRegister
            when 2
                arguments[:lhs] = virtualRegister
                arguments[:rhs] = virtualRegister
            end

            context.eval("Proc.new { |arguments, extras | op('#{name}', { extras: extras, args: arguments }) }").call(arguments, value)
        end
    end

    def self.generate_llint_generator(section)
        opcodes = section.opcodes.select { |op| ["arithmetic", "comparison", "conversion"].include? op.extras["category"] }
        methods = opcodes.map do |op|
            case op.args.size
            when 2
                generate_unary_op(op)
            when 3
                generate_binary_op(op)
            else
                assert("Invalid argument count #{op.args.size} for op #{op.name}") { false }
            end
        end
        methods.join("\n")
    end

    def self.generate_binary_op(op)
        <<-EOF
template<>
auto LLIntGenerator::addOp<#{op_type(op)}>(ExpressionType lhs, ExpressionType rhs, ExpressionType& result) -> PartialResult
{
    result = push();
    #{op.capitalized_name}::emit(this, result, lhs, rhs);
    return { };
}
        EOF
    end

    def self.generate_unary_op(op)
        <<-EOF
template<>
auto LLIntGenerator::addOp<#{op_type(op)}>(ExpressionType operand, ExpressionType& result) -> PartialResult
{
    result = push();
    #{op.capitalized_name}::emit(this, result, operand);
    return { };
}
        EOF
    end

    def self.op_type(op)
        "OpType::#{op.unprefixed_name.gsub(/^.|[^a-z0-9]./) { |c| c[-1].upcase }}"
    end
end
