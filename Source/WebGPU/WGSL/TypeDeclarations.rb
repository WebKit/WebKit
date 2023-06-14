# FIXME: add all the missing type declarations here
operator :+, {
    [T < Number].(T, T) => T,

    # vector addition
    [T < Number, N].(Vector[T, N], T) => Vector[T, N],
    [T < Number, N].(T, Vector[T, N]) => Vector[T, N],
    [T < Number, N].(Vector[T, N], Vector[T, N]) => Vector[T, N],

    # matrix-matrix addition
    [T < Float, C, R].(Matrix[T, C, R], Matrix[T, C, R]) => Matrix[T, C, R],
}

operator :-, {
    # unary
    [T < SignedNumber].(T) => T,
    [T < SignedNumber, N].(Vector[T, N]) => Vector[T, N],

    # binary
    [T < Number].(T, T) => T,
    [T < Number, N].(Vector[T, N], T) => Vector[T, N],
    [T < Number, N].(T, Vector[T, N]) => Vector[T, N],
    [T < Number, N].(Vector[T, N], Vector[T, N]) => Vector[T, N],
}

operator :*, {
    [T < Number].(T, T) => T,

    # vector scaling
    [T < Number, N].(Vector[T, N], T) => Vector[T, N],
    [T < Number, N].(T, Vector[T, N]) => Vector[T, N],
    [T < Number, N].(Vector[T, N], Vector[T, N]) => Vector[T, N],

    # matrix-vector multiplication
    [T < Float, C, R].(Matrix[T, C, R], Vector[T, C]) => Vector[T, R],
    [T < Float, C, R].(Vector[T, R], Matrix[T, C, R]) => Vector[T, C],

    # matrix-matrix multiplication
    [T < Float, C, R, K].(Matrix[T, K, R], Matrix[T, C, K]) => Matrix[T, C, R],
}

operator :/, {
    [T < Number].(T, T) => T,

    # vector scaling
    [T < Number, N].(Vector[T, N], T) => Vector[T, N],
    [T < Number, N].(T, Vector[T, N]) => Vector[T, N],
    [T < Number, N].(Vector[T, N], Vector[T, N]) => Vector[T, N],
}

operator :%, {
    [T < Number].(T, T) => T,

    # vector scaling
    [T < Number, N].(Vector[T, N], T) => Vector[T, N],
    [T < Number, N].(T, Vector[T, N]) => Vector[T, N],
    [T < Number, N].(Vector[T, N], Vector[T, N]) => Vector[T, N],
}

# Comparison operations
["==", "!="].each do |op|
    operator :"#{op}", {
        [T < Scalar].(T, T) => Bool,
        [T < Scalar, N].(Vector[T, N], Vector[T, N]) => Vector[Bool, N],
    }
end

["<", "<=", ">", ">="].each do |op|
    operator :"#{op}", {
        [T < Number].(T, T) => Bool,
        [T < Number, N].(Vector[T, N], Vector[T, N]) => Vector[Bool, N],
    }
end

# Bitwise operations
operator :~, {
    [T < Integer].(T) => T,
    [T < Integer, N].(Vector[T, N]) => Vector[T, N]
}

["|", "&", "^"].each do |op|
    operator :"#{op}", {
        [T < Integer].(T, T) => T,
        [T < Integer, N].(Vector[T, N], Vector[T, N]) => Vector[T, N]
    }
end

operator :textureSample, {
    [].(Texture[F32, Texture1d], Sampler, F32) => Vector[F32, 4],
    [].(Texture[F32, Texture2d], Sampler, Vector[F32, 2]) => Vector[F32, 4],
    [].(Texture[F32, Texture2d], Sampler, Vector[F32, 2], Vector[I32, 2]) => Vector[F32, 4],
    [T < ConcreteInteger].(Texture[F32, Texture2dArray], Sampler, Vector[F32, 2], T) => Vector[F32, 4],
    [T < ConcreteInteger].(Texture[F32, Texture2dArray], Sampler, Vector[F32, 2], T, Vector[I32, 2]) => Vector[F32, 4],
    [].(Texture[F32, Texture3d], Sampler, Vector[F32, 3]) => Vector[F32, 4],
    [].(Texture[F32, TextureCube], Sampler, Vector[F32, 3]) => Vector[F32, 4],
    [].(Texture[F32, Texture3d], Sampler, Vector[F32, 3], Vector[I32, 3]) => Vector[F32, 4],
    [T < ConcreteInteger].(Texture[F32, TextureCubeArray], Sampler, Vector[F32, 3], T) => Vector[F32, 4],

    # FIXME: add overloads for texture_depth
    # https://bugs.webkit.org/show_bug.cgi?id=254515
}

operator :textureSampleBaseClampToEdge, {
  [].(TextureExternal, Sampler, Vector[F32, 2]) => Vector[F32, 4],
  [].(Texture[F32, Texture2d], Sampler, Vector[F32, 2]) => Vector[F32, 4],
}

# 16.1. Constructor Built-in Functions

# 16.1.1. Zero Value Built-in Functions
operator :bool, {
    [].() => Bool,
}
operator :i32, {
    [].() => I32,
}
operator :u32, {
    [].() => U32,
}
operator :f32, {
    [].() => F32,
}
operator :vec2, {
    [T < ConcreteScalar].() => Vector[T, 2],
}
operator :vec3, {
    [T < ConcreteScalar].() => Vector[T, 3],
}
operator :vec4, {
    [T < ConcreteScalar].() => Vector[T, 4],
}
(2..4).each do |columns|
    (2..4).each do |rows|
        operator :"mat#{columns}x#{rows}", {
            [T < ConcreteFloat].() => Matrix[T, columns, rows],
        }
    end
end

# 16.1.2. Value Constructor Built-in Functions

# 16.1.2.1. array
# Implemented inline in the type checker

# 16.1.2.2.
operator :bool, {
    [T < ConcreteScalar].(T) => Bool,
}

# 16.1.2.3. f16
# FIXME: add support for f16

# 16.1.2.4.
operator :f32, {
    [T < ConcreteScalar].(T) => F32,
}

# 16.1.2.5.
operator :i32, {
    [T < ConcreteScalar].(T) => I32,
}

# 16.1.2.6 - 14: matCxR
(2..4).each do |columns|
    (2..4).each do |rows|
        operator :"mat#{columns}x#{rows}", {
            [T < ConcreteFloat, S < Float].(Matrix[S, columns, rows]) => Matrix[T, columns, rows],
            [T < Float].(Matrix[T, columns, rows]) => Matrix[T, columns, rows],
            [T < Float].(*([T] * columns * rows)) => Matrix[T, columns, rows],
            [T < Float].(*([Vector[T, rows]] * columns)) => Matrix[T, columns, rows],
        }
    end
end

# 16.1.2.15. Structures
# Implemented inline in the type checker

# 16.1.2.16.
operator :u32, {
    [T < ConcreteScalar].(T) => U32,
}

# 16.1.2.17.
operator :vec2, {
    [T < Scalar].(T) => Vector[T, 2],
    [T < ConcreteScalar, S < Scalar].(Vector[S, 2]) => Vector[T, 2],
    [S < Scalar].(Vector[S, 2]) => Vector[S, 2],
    [T < Scalar].(T, T) => Vector[T, 2],
    [].() => Vector[AbstractInt, 2],
}

# 16.1.2.18.
operator :vec3, {
    [T < Scalar].(T) => Vector[T, 3],
    [T < ConcreteScalar, S < Scalar].(Vector[S, 3]) => Vector[T, 3],
    [S < Scalar].(Vector[S, 3]) => Vector[S, 3],
    [T < Scalar].(T, T, T) => Vector[T, 3],
    [T < Scalar].(Vector[T, 2], T) => Vector[T, 3],
    [T < Scalar].(T, Vector[T, 2]) => Vector[T, 3],
    [].() => Vector[AbstractInt, 3],
}

# 16.1.2.19.
operator :vec4, {
    [T < Scalar].(T) => Vector[T, 4],
    [T < ConcreteScalar, S < Scalar].(Vector[S, 4]) => Vector[T, 4],
    [S < Scalar].(Vector[S, 4]) => Vector[S, 4],
    [T < Scalar].(T, T, T, T) => Vector[T, 4],
    [T < Scalar].(T, Vector[T, 2], T) => Vector[T, 4],
    [T < Scalar].(T, T, Vector[T, 2]) => Vector[T, 4],
    [T < Scalar].(Vector[T, 2], T, T) => Vector[T, 4],
    [T < Scalar].(Vector[T, 2], Vector[T, 2]) => Vector[T, 4],
    [T < Scalar].(Vector[T, 3], T) => Vector[T, 4],
    [T < Scalar].(T, Vector[T, 3]) => Vector[T, 4],
    [].() => Vector[AbstractInt, 4],
}

# 7.6. Logical Expressions (https://gpuweb.github.io/gpuweb/wgsl/#logical-expr)

operator :!, {
    [].(Bool) => Bool,
    [N].(Vector[Bool, N]) => Vector[Bool, N],
}

operator :'||', {
    [].(Bool, Bool) => Bool,
}

operator :'&&', {
    [].(Bool, Bool) => Bool,
}

operator :'|', {
    [].(Bool, Bool) => Bool,
    [N].(Vector[Bool, N], Vector[Bool, N]) => Vector[Bool, N],
}

operator :'&', {
    [].(Bool, Bool) => Bool,
    [N].(Vector[Bool, N], Vector[Bool, N]) => Vector[Bool, N],
}

# 17.3. Logical Built-in Functions (https://www.w3.org/TR/WGSL/#logical-builtin-functions)

# 17.3.1 & 17.3.2
["all", "any"].each do |op|
    operator :"#{op}", {
        [N].(Vector[Bool, N]) => Bool,
        [N].(Bool) => Bool,
    }
end

# 17.3.3
operator :select, {
    [T < Scalar].(T, T, Bool) => T,
    [T < Scalar, N].(Vector[T, N], Vector[T, N], Bool) => Vector[T, N],
    [T < Scalar, N].(Vector[T, N], Vector[T, N], Vector[Bool, N]) => Vector[T, N],
}

# 17.5. Numeric Built-in Functions (https://www.w3.org/TR/WGSL/#numeric-builtin-functions)

# Trigonometric
["acos", "asin", "atan", "cos", "sin", "tan",
 "acosh", "asinh", "atanh", "cosh", "sinh", "tanh"].each do |op|
    operator :"#{op}", {
        [T < Float].(T) => T,
        [T < Float, N].(Vector[T, N]) => Vector[T, N],
    }
end

# 17.5.1
operator :abs, {
    [T < Number].(T) => T,
    [T < Number, N].(Vector[T, N]) => Vector[T, N],
}

# 17.5.2. acos
# 17.5.3. acosh
# 17.5.4. asin
# 17.5.5. asinh
# 17.5.6. atan
# 17.5.7. atanh
# Defined in [Trigonometric]

# 17.5.8
operator :atan2, {
    [T < Float].(T, T) => T,
    [T < Float, N].(Vector[T, N], Vector[T, N]) => Vector[T, N],
}

# 17.5.9
operator :ceil, {
    [T < Float].(T) => T,
    [T < Float, N].(Vector[T, N]) => Vector[T, N],
}

# 17.5.10
operator :clamp, {
    [T < Number].(T, T, T) => T,
    [T < Number, N].(Vector[T, N], Vector[T, N], Vector[T, N]) => Vector[T, N],
}

# 17.5.11. cos
# 17.5.12. cosh
# Defined in [Trigonometric]

# 17.5.13-15 (Bit counting)
["countLeadingZeros", "countOneBits", "countTrailingZeros"].each do |op|
    operator :"#{op}", {
        [T < ConcreteInteger].(T) => T,
        [T < ConcreteInteger, N].(Vector[T, N]) => Vector[T, N],
    }
end

# 17.5.16
operator :cross, {
    [T < Float].(Vector[T, 3], Vector[T, 3]) => Vector[T, 3],
}

# 17.5.17
operator :degrees, {
    [T < Float].(T) => T,
    [T < Float, N].(Vector[T, N]) => Vector[T, N],
}

# 17.5.18
operator :determinant, {
    [T < Float, C].(Matrix[T, C, C]) => T,
}

# 17.5.19
operator :distance, {
    [T < Float].(T, T) => T,
    [T < Float, N].(Vector[T, N], Vector[T, N]) => T,
}

# 17.5.20
operator :dot, {
    [T < Number, N].(Vector[T, N], Vector[T, N]) => T
}

# 17.5.21 & 17.5.22
["exp", "exp2"].each do |op|
    operator :"#{op}", {
        [T < Float].(T) => T,
        [T < Float, N].(Vector[T, N]) => Vector[T, N],
    }
end

# 17.5.23 & 17.5.24
operator :extractBits, {
    # signed
    [].(I32, U32, U32) => I32,
    [N].(Vector[I32, N], U32, U32) => Vector[I32, N],

    # unsigned
    [].(U32, U32, U32) => U32,
    [N].(Vector[U32, N], U32, U32) => Vector[U32, N],
}

# 17.5.25
operator :faceForward, {
    [T < Float, N].(Vector[T, N], Vector[T, N], Vector[T, N]) => Vector[T, N],
}

# 17.5.26 & 17.5.27
operator :firstLeadingBit, {
    # signed
    [].(I32) => I32,
    [N].(Vector[I32, N]) => Vector[I32, N],

    # unsigned
    [].(U32) => U32,
    [N].(Vector[U32, N]) => Vector[U32, N],
}

# 17.5.28
operator :firstTrailingBit, {
    [T < ConcreteInteger].(T) => T,
    [T < ConcreteInteger, N].(Vector[T, N]) => Vector[T, N],
}

# 17.5.29
operator :floor, {
    [T < Float].(T) => T,
    [T < Float, N].(Vector[T, N]) => Vector[T, N],
}

# 17.5.30
operator :fma, {
    [T < Float].(T, T, T) => T,
    [T < Float, N].(Vector[T, N], Vector[T, N], Vector[T, N]) => Vector[T, N],
}

# 17.5.31
operator :fract, {
    [T < Float].(T) => T,
    [T < Float, N].(Vector[T, N]) => Vector[T, N],
}

# 17.5.32
operator :frexp, {
    # FIXME: this needs the special return types __frexp_result_*
}

# 17.5.33
operator :insertBits, {
    [T < ConcreteInteger].(T, T, U32, U32) => T,
    [T < ConcreteInteger, N].(Vector[T, N], Vector[T, N], U32, U32) => Vector[T, N],
}

# 17.5.34
operator :inverseSqrt, {
    [T < Float].(T) => T,
    [T < Float, N].(Vector[T, N]) => Vector[T, N],
}

# 17.5.35
operator :ldexp, {
    [T < ConcreteFloat].(T, I32) => T,
    [].(AbstractFloat, AbstractInt) => AbstractFloat,
    [T < ConcreteFloat, N].(Vector[T, N], Vector[I32, N]) => Vector[T, N],
    [N].(Vector[AbstractFloat, N], Vector[AbstractInt, N]) => Vector[AbstractFloat, N],
}

# 17.5.36
operator :length, {
    [T < Float].(T) => T,
    [T < Float, N].(Vector[T, N]) => T,
}

# 17.5.37 & 17.5.38
["log", "log2"].each do |op|
    operator :"#{op}", {
        [T < Float].(T) => T,
        [T < Float, N].(Vector[T, N]) => Vector[T, N],
    }
end

# 17.5.39
operator :max, {
    [T < Number].(T, T) => T,
    [T < Number, N].(Vector[T, N], Vector[T, N]) => Vector[T, N],
}

# 17.5.40
operator :min, {
    [T < Number].(T, T) => T,
    [T < Number, N].(Vector[T, N], Vector[T, N]) => Vector[T, N],
}

# 17.5.41
operator :mix, {
    [T < Float].(T, T, T) => T,
    [T < Float, N].(Vector[T, N], Vector[T, N], Vector[T, N]) => Vector[T, N],
    [T < Float, N].(Vector[T, N], Vector[T, N], T) => Vector[T, N],
}

# 17.5.42
operator :modf, {
    # FIXME: this needs the special return types __modf_result_*
}

# 17.5.43
operator :normalize, {
    [T < Float, N].(Vector[T, N]) => Vector[T, N],
}

# 17.5.44
operator :pow, {
    [T < Float].(T, T) => T,
    [T < Float, N].(Vector[T, N], Vector[T, N]) => Vector[T, N],
}

# 17.5.45
operator :quantizeToF16, {
    [].(F32) => F32,
    [N].(Vector[F32, N]) => Vector[F32, N],
}

# 17.5.46
operator :radians, {
    [T < Float].(T) => T,
    [T < Float, N].(Vector[T, N]) => Vector[T, N],
}

# 17.5.47
operator :reflect, {
    [T < Float, N].(Vector[T, N], Vector[T, N]) => Vector[T, N],
}

# 17.5.48
operator :refract, {
    [T < Float, N].(Vector[T, N], Vector[T, N], T) => Vector[T, N],
}

# 17.5.49
operator :reverseBits, {
    [T < ConcreteInteger].(T) => T,
    [T < ConcreteInteger, N].(Vector[T, N]) => Vector[T, N],
}

# 17.5.50
operator :round, {
    [T < Float].(T) => T,
    [T < Float, N].(Vector[T, N]) => Vector[T, N],
}

# 17.5.51
operator :saturate, {
    [T < Float].(T) => T,
    [T < Float, N].(Vector[T, N]) => Vector[T, N],
}

# 17.5.52
operator :sign, {
    [T < SignedNumber].(T) => T,
    [T < SignedNumber, N].(Vector[T, N]) => Vector[T, N],
}

# 17.5.53. sin
# 17.5.54. sinh
# Defined in [Trigonometric]

# 17.5.55
operator :smoothstep, {
    [T < Float].(T, T, T) => T,
    [T < Float, N].(Vector[T, N], Vector[T, N], Vector[T, N]) => Vector[T, N],
}

# 17.5.56
operator :sqrt, {
    [T < Float].(T) => T,
    [T < Float, N].(Vector[T, N]) => Vector[T, N],
}

# 17.5.57
operator :step, {
    [T < Float].(T, T) => T,
    [T < Float, N].(Vector[T, N], Vector[T, N]) => Vector[T, N],
}

# 17.5.58. tan
# 17.5.59. tanh
# Defined in [Trigonometric]

# 17.5.60
operator :transpose, {
    [T < Float, C, R].(Matrix[T, C, R]) => Matrix[T, R, C],
}

# 17.5.61
operator :trunc, {
    [T < Float].(T) => T,
    [T < Float, N].(Vector[T, N]) => Vector[T, N],
}

# 17.7. Texture Built-in Functions (https://gpuweb.github.io/gpuweb/wgsl/#texture-builtin-functions)

# 17.7.4
operator :textureLoad, {
    [T < ConcreteInteger, U < ConcreteInteger, S < Concrete32BitNumber].(Texture[S, Texture1d], T, U) => Vector[S, 4],
    [T < ConcreteInteger, U < ConcreteInteger, S < Concrete32BitNumber].(Texture[S, Texture2d], Vector[T, 2], U) => Vector[S, 4],
    [T < ConcreteInteger, V < ConcreteInteger, U < ConcreteInteger, S < Concrete32BitNumber].(Texture[S, Texture2dArray], Vector[T, 2], V, U) => Vector[S, 4],
    [T < ConcreteInteger, U < ConcreteInteger, S < Concrete32BitNumber].(Texture[S, Texture3d], Vector[T, 3], U) => Vector[S, 4],
    [T < ConcreteInteger, U < ConcreteInteger, S < Concrete32BitNumber].(Texture[S, TextureMultisampled2d], Vector[T, 2], U) => Vector[S, 4],

    [T < ConcreteInteger].(TextureExternal, Vector[T, 2]) => Vector[F32, 4],

    # FIXME: add overloads for texture_depth
    # https://bugs.webkit.org/show_bug.cgi?id=254515
}
