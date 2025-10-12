require 'json'

module Wasm
    def self.normalize(name)
        name.gsub(/(\.|\/)/, "_")
    end

    def self.autogenerate_opcodes(context, wasm_json)
    end

    def self.generate_llint_generator(section)
        <<-EOF
        EOF
    end

    def self.generate_binary_op(op)
        <<-EOF
        EOF
    end

    def self.generate_unary_op(op)
        <<-EOF
        EOF
    end

    def self.unprefixed_capitalized_name(op)
        op.unprefixed_name.gsub(/^.|[^a-z0-9]./) { |c| c[-1].upcase }
    end
end
