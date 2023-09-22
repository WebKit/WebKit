# FIXME: add all the missing type declarations here

suffixes = {
    f: F32,
    i: I32,
    u: U32,
}

[2, 3, 4].each do |n|
    suffixes.each do |suffix, type|
        type_alias :"vec#{n}#{suffix}", Vector[type, n]
    end

    [2, 3, 4].each do |m|
        # FIXME: add alias for F16 once we support it
        type_alias :"mat#{n}x#{m}f", Matrix[F32, n, m]
    end
end

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
    # unary
    [AS, T, AM].(Ptr[AS, T, AM]) => Ref[AS, T, AM],

    # binary
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

# 8.6. Logical Expressions (https://gpuweb.github.io/gpuweb/wgsl/#logical-expr)

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
    # unary
    [AS, T, AM].(Ref[AS, T, AM]) => Ptr[AS, T, AM],

    # binary
    [].(Bool, Bool) => Bool,
    [N].(Vector[Bool, N], Vector[Bool, N]) => Vector[Bool, N],
}

# 8.9. Bit Expressions (https://www.w3.org/TR/WGSL/#bit-expr)

operator :~, {
    [T < Integer].(T) => T,
    [T < Integer, N].(Vector[T, N]) => Vector[T, N]
}

["|", "&", "^", "<<", ">>"].each do |op|
    operator :"#{op}", {
        [T < Integer].(T, T) => T,
        [T < Integer, N].(Vector[T, N], Vector[T, N]) => Vector[T, N]
    }
end

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

# 16.4. Array Built-in Functions

# 16.4.1.
operator :arrayLength, {
  [T].(Ptr[Storage, Array[T], Read]) => U32,
  [T].(Ptr[Storage, Array[T], ReadWrite]) => U32,
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

# 16.7. Texture Built-in Functions (https://gpuweb.github.io/gpuweb/wgsl/#texture-builtin-functions)

# 16.7.1
operator :textureDimensions, {
    # ST is i32, u32, or f32
    # F is a texel format
    # A is an access mode
    # T is texture_1d<ST> or texture_storage_1d<F,A>
    # @must_use fn textureDimensions(t: T) -> u32
    [S < Concrete32BitNumber].(Texture[S, Texture1d]) => U32,
    # FIXME: add declarations for texture storage

    # ST is i32, u32, or f32
    # T is texture_1d<ST>
    # L is i32, or u32
    # @must_use fn textureDimensions(t: T, level: L) -> u32
    [S < Concrete32BitNumber, T < ConcreteInteger].(Texture[S, Texture1d], T) => U32,

    # ST is i32, u32, or f32
    # F is a texel format
    # A is an access mode
    # T is texture_2d<ST>, texture_2d_array<ST>, texture_cube<ST>, texture_cube_array<ST>, texture_multisampled_2d<ST>, texture_depth_2d, texture_depth_2d_array, texture_depth_cube, texture_depth_cube_array, texture_depth_multisampled_2d, texture_storage_2d<F,A>, texture_storage_2d_array<F,A>, or texture_external
    # @must_use fn textureDimensions(t: T) -> vec2<u32>
    [S < Concrete32BitNumber].(Texture[S, Texture2d]) => Vector[U32, 2],
    [S < Concrete32BitNumber].(Texture[S, Texture2dArray]) => Vector[U32, 2],
    [S < Concrete32BitNumber].(Texture[S, TextureCube]) => Vector[U32, 2],
    [S < Concrete32BitNumber].(Texture[S, TextureCubeArray]) => Vector[U32, 2],
    [S < Concrete32BitNumber].(Texture[S, TextureMultisampled2d]) => Vector[U32, 2],
    [].(texture_depth_2d) => Vector[U32, 2],
    [].(texture_depth_2d_array) => Vector[U32, 2],
    [].(texture_depth_cube) => Vector[U32, 2],
    [].(texture_depth_cube_array) => Vector[U32, 2],
    [].(texture_depth_multisampled_2d) => Vector[U32, 2],
    # FIXME: add declarations for texture storage
    [].(TextureExternal) => Vector[U32, 2],

    # ST is i32, u32, or f32
    # T is texture_2d<ST>, texture_2d_array<ST>, texture_cube<ST>, texture_cube_array<ST>, texture_depth_2d, texture_depth_2d_array, texture_depth_cube, or texture_depth_cube_array
    # L is i32, or u32
    # @must_use fn textureDimensions(t: T, level: L) -> vec2<u32>
    [S < Concrete32BitNumber, T < ConcreteInteger].(Texture[S, Texture2d], T) => Vector[U32, 2],
    [S < Concrete32BitNumber, T < ConcreteInteger].(Texture[S, Texture2dArray], T) => Vector[U32, 2],
    [S < Concrete32BitNumber, T < ConcreteInteger].(Texture[S, TextureCube], T) => Vector[U32, 2],
    [S < Concrete32BitNumber, T < ConcreteInteger].(Texture[S, TextureCubeArray], T) => Vector[U32, 2],
    [T < ConcreteInteger].(texture_depth_2d, T) => Vector[U32, 2],
    [T < ConcreteInteger].(texture_depth_2d_array, T) => Vector[U32, 2],
    [T < ConcreteInteger].(texture_depth_cube, T) => Vector[U32, 2],
    [T < ConcreteInteger].(texture_depth_cube_array, T) => Vector[U32, 2],

    # ST is i32, u32, or f32
    # F is a texel format
    # A is an access mode
    # T is texture_3d<ST> or texture_storage_3d<F,A>
    # @must_use fn textureDimensions(t: T) -> vec3<u32>
    [S < Concrete32BitNumber].(Texture[S, Texture3d]) => Vector[U32, 3],
    # FIXME: add declarations for texture storage

    # ST is i32, u32, or f32
    # T is texture_3d<ST>
    # L is i32, or u32
    # @must_use fn textureDimensions(t: T, level: L) -> vec3<u32>
    [S < Concrete32BitNumber, T < ConcreteInteger].(Texture[S, Texture3d], T) => Vector[U32, 3],
}

# 16.7.2
operator :textureGather, {
    [T < ConcreteInteger, S < Concrete32BitNumber].(T, Texture[S, Texture2d], Sampler, Vector[F32, 2]) => Vector[S, 4],

    [T < ConcreteInteger, S < Concrete32BitNumber].(T, Texture[S, Texture2d], Sampler, Vector[F32, 2], Vector[I32, 2]) => Vector[S, 4],

    [T < ConcreteInteger, S < Concrete32BitNumber, U < ConcreteInteger].(T, Texture[S, Texture2dArray], Sampler, Vector[F32, 2], U) => Vector[S, 4],

    [T < ConcreteInteger, S < Concrete32BitNumber, U < ConcreteInteger].(T, Texture[S, Texture2dArray], Sampler, Vector[F32, 2], U, Vector[I32, 2]) => Vector[S, 4],

    [T < ConcreteInteger, S < Concrete32BitNumber].(T, Texture[S, TextureCube], Sampler, Vector[F32, 3]) => Vector[S, 4],

    [T < ConcreteInteger, S < Concrete32BitNumber, U < ConcreteInteger].(T, Texture[S, TextureCubeArray], Sampler, Vector[F32, 3], U) => Vector[S, 4],

    # @must_use fn textureGather(t: texture_depth_2d, s: sampler, coords: vec2<f32>) -> vec4<f32>
    [].(texture_depth_2d, Sampler, Vector[F32, 2]) => Vector[F32, 4],

    # @must_use fn textureGather(t: texture_depth_2d, s: sampler, coords: vec2<f32>, offset: vec2<i32>) -> vec4<f32>
    [].(texture_depth_2d, Sampler, Vector[F32, 2], Vector[I32, 2]) => Vector[F32, 4],

    # @must_use fn textureGather(t: texture_depth_cube, s: sampler, coords: vec3<f32>) -> vec4<f32>
    [].(texture_depth_cube, Sampler, Vector[F32, 3]) => Vector[F32, 4],

    # A is i32, or u32
    # @must_use fn textureGather(t: texture_depth_2d_array, s: sampler, coords: vec2<f32>, array_index: A) -> vec4<f32>
    [U < ConcreteInteger].(texture_depth_2d_array, Sampler, Vector[F32, 2], U) => Vector[F32, 4],

    # A is i32, or u32
    # @must_use fn textureGather(t: texture_depth_2d_array, s: sampler, coords: vec2<f32>, array_index: A, offset: vec2<i32>) -> vec4<f32>
    [U < ConcreteInteger].(texture_depth_2d_array, Sampler, Vector[F32, 2], U, Vector[I32, 2]) => Vector[F32, 4],

    # A is i32, or u32
    # @must_use fn textureGather(t: texture_depth_cube_array, s: sampler, coords: vec3<f32>, array_index: A) -> vec4<f32>
    [U < ConcreteInteger].(texture_depth_cube_array, Sampler, Vector[F32, 3], U) => Vector[F32, 4],
}

# 16.7.3 textureGatherCompare
# FIXME: Implement sampler_comparison

# 16.7.4
operator :textureLoad, {
    [T < ConcreteInteger, U < ConcreteInteger, S < Concrete32BitNumber].(Texture[S, Texture1d], T, U) => Vector[S, 4],
    [T < ConcreteInteger, U < ConcreteInteger, S < Concrete32BitNumber].(Texture[S, Texture2d], Vector[T, 2], U) => Vector[S, 4],
    [T < ConcreteInteger, V < ConcreteInteger, U < ConcreteInteger, S < Concrete32BitNumber].(Texture[S, Texture2dArray], Vector[T, 2], V, U) => Vector[S, 4],
    [T < ConcreteInteger, U < ConcreteInteger, S < Concrete32BitNumber].(Texture[S, Texture3d], Vector[T, 3], U) => Vector[S, 4],
    [T < ConcreteInteger, U < ConcreteInteger, S < Concrete32BitNumber].(Texture[S, TextureMultisampled2d], Vector[T, 2], U) => Vector[S, 4],


    # C is i32, or u32
    # L is i32, or u32
    # @must_use fn textureLoad(t: texture_depth_2d, coords: vec2<C>, level: L) -> f32
    [T < ConcreteInteger, U < ConcreteInteger].(texture_depth_2d, Vector[T, 2], U) => F32,

    # C is i32, or u32
    # A is i32, or u32
    # L is i32, or u32
    # @must_use fn textureLoad(t: texture_depth_2d_array, coords: vec2<C>, array_index: A, level: L) -> f32
    [T < ConcreteInteger, S < ConcreteInteger, U < ConcreteInteger].(texture_depth_2d_array, Vector[T, 2], S, U) => F32,

    # C is i32, or u32
    # S is i32, or u32
    # @must_use fn textureLoad(t: texture_depth_multisampled_2d, coords: vec2<C>, sample_index: S)-> f32
    [T < ConcreteInteger, U < ConcreteInteger].(texture_depth_multisampled_2d, Vector[T, 2], U) => F32,

    [T < ConcreteInteger].(TextureExternal, Vector[T, 2]) => Vector[F32, 4],
}

# 16.7.5
operator :textureNumLayers, {
    # F is a texel format
    # A is an access mode
    # ST is i32, u32, or f32
    # T is texture_2d_array<ST>, texture_cube_array<ST>, texture_depth_2d_array, texture_depth_cube_array, or texture_storage_2d_array<F,A>
    # @must_use fn textureNumLayers(t: T) -> u32
    [S < Concrete32BitNumber].(Texture[S, Texture2dArray]) => U32,
    [S < Concrete32BitNumber].(Texture[S, TextureCubeArray]) => U32,
    [].(texture_depth_2d_array) => U32,
    [].(texture_depth_cube_array) => U32,
    # FIXME: add declarations for texture storage
}

# 16.7.6
operator :textureNumLevels, {
    # ST is i32, u32, or f32
    # T is texture_1d<ST>, texture_2d<ST>, texture_2d_array<ST>, texture_3d<ST>, texture_cube<ST>, texture_cube_array<ST>, texture_depth_2d, texture_depth_2d_array, texture_depth_cube, or texture_depth_cube_array
    # @must_use fn textureNumLevels(t: T) -> u32
    [S < Concrete32BitNumber].(Texture[S, Texture1d]) => U32,
    [S < Concrete32BitNumber].(Texture[S, Texture2d]) => U32,
    [S < Concrete32BitNumber].(Texture[S, Texture2dArray]) => U32,
    [S < Concrete32BitNumber].(Texture[S, Texture3d]) => U32,
    [S < Concrete32BitNumber].(Texture[S, TextureCube]) => U32,
    [S < Concrete32BitNumber].(Texture[S, TextureCubeArray]) => U32,
    [].(texture_depth_2d) => U32,
    [].(texture_depth_2d_array) => U32,
    [].(texture_depth_cube) => U32,
    [].(texture_depth_cube_array) => U32,
}

# 16.7.7
operator :textureNumSamples, {
    # ST is i32, u32, or f32
    # T is texture_multisampled_2d<ST> or texture_depth_multisampled_2d
    # @must_use fn textureNumSamples(t: T) -> u32
    [S < Concrete32BitNumber].(Texture[S, TextureMultisampled2d]) => U32,
    [].(texture_depth_multisampled_2d) => U32,
}

# 16.7.8
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

    # @must_use fn textureSample(t: texture_depth_2d, s: sampler, coords: vec2<f32>) -> f32
    [].(texture_depth_2d, Sampler, Vector[F32, 2]) => F32,

    # @must_use fn textureSample(t: texture_depth_2d, s: sampler, coords: vec2<f32>, offset: vec2<i32>) -> f32
    [].(texture_depth_2d, Sampler, Vector[F32, 2], Vector[I32, 2]) => F32,

    # A is i32, or u32
    # @must_use fn textureSample(t: texture_depth_2d_array, s: sampler, coords: vec2<f32>, array_index: A) -> f32
    [T < ConcreteInteger].(texture_depth_2d_array, Sampler, Vector[F32, 2], T) => F32,

    # A is i32, or u32
    # @must_use fn textureSample(t: texture_depth_2d_array, s: sampler, coords: vec2<f32>, array_index: A, offset: vec2<i32>) -> f32
    [T < ConcreteInteger].(texture_depth_2d_array, Sampler, Vector[F32, 2], T, Vector[I32, 2]) => F32,

    # @must_use fn textureSample(t: texture_depth_cube, s: sampler, coords: vec3<f32>) -> f32
    [].(texture_depth_cube, Sampler, Vector[F32, 3]) => F32,

    # A is i32, or u32
    # @must_use fn textureSample(t: texture_depth_cube_array, s: sampler, coords: vec3<f32>, array_index: A) -> f32
    [T < ConcreteInteger].(texture_depth_cube_array, Sampler, Vector[F32, 3], T) => F32,
}

# 16.7.9
operator :textureSampleBias, {
    [].(Texture[F32, Texture2d], Sampler, Vector[F32, 2], F32) => Vector[F32, 4],

    [].(Texture[F32, Texture2d], Sampler, Vector[F32, 2], F32, Vector[I32, 2]) => Vector[F32, 4],

    [T < ConcreteInteger].(Texture[F32, Texture2dArray], Sampler, Vector[F32, 2], T, F32) => Vector[F32, 4],

    [T < ConcreteInteger].(Texture[F32, Texture2dArray], Sampler, Vector[F32, 2], T, F32, Vector[I32, 2]) => Vector[F32, 4],

    [].(Texture[F32, Texture3d], Sampler, Vector[F32, 3], F32) => Vector[F32, 4],
    [].(Texture[F32, TextureCube], Sampler, Vector[F32, 3], F32) => Vector[F32, 4],

    [].(Texture[F32, Texture3d], Sampler, Vector[F32, 3], F32, Vector[I32, 3]) => Vector[F32, 4],

    [T < ConcreteInteger].(Texture[F32, TextureCubeArray], Sampler, Vector[F32, 3], T, F32) => Vector[F32, 4],
}

# 16.7.10 textureSampleCompare
# FIXME: Implement sampler_comparison

# 16.7.11 textureSampleCompareLevel
# FIXME: Implement sampler_comparison

# 16.7.12
operator :textureSampleGrad, {
    [].(Texture[F32, Texture2d], Sampler, Vector[F32, 2], Vector[F32, 2], Vector[F32, 2]) => Vector[F32, 4],

    [].(Texture[F32, Texture2d], Sampler, Vector[F32, 2], Vector[F32, 2], Vector[F32, 2], Vector[I32, 2]) => Vector[F32, 4],

    [T < ConcreteInteger].(Texture[F32, Texture2dArray], Sampler, Vector[F32, 2], T, Vector[F32, 2], Vector[F32, 2]) => Vector[F32, 4],

    [T < ConcreteInteger].(Texture[F32, Texture2dArray], Sampler, Vector[F32, 2], T, Vector[F32, 2], Vector[F32, 2], Vector[I32, 2]) => Vector[F32, 4],

    [].(Texture[F32, Texture3d], Sampler, Vector[F32, 3], Vector[F32, 3], Vector[F32, 3]) => Vector[F32, 4],
    [].(Texture[F32, TextureCube], Sampler, Vector[F32, 3], Vector[F32, 3], Vector[F32, 3]) => Vector[F32, 4],

    [].(Texture[F32, Texture3d], Sampler, Vector[F32, 3], Vector[F32, 3], Vector[F32, 3], Vector[I32, 3]) => Vector[F32, 4],

    [T < ConcreteInteger].(Texture[F32, TextureCubeArray], Sampler, Vector[F32, 3], T, Vector[F32, 3], Vector[F32, 3]) => Vector[F32, 4],
}

# 16.7.13
operator :textureSampleLevel, {
    [].(Texture[F32, Texture2d], Sampler, Vector[F32, 2], F32) => Vector[F32, 4],

    [].(Texture[F32, Texture2d], Sampler, Vector[F32, 2], F32, Vector[I32, 2]) => Vector[F32, 4],

    [T < ConcreteInteger].(Texture[F32, Texture2dArray], Sampler, Vector[F32, 2], T, F32) => Vector[F32, 4],

    [T < ConcreteInteger].(Texture[F32, Texture2dArray], Sampler, Vector[F32, 2], T, F32, Vector[I32, 2]) => Vector[F32, 4],

    [].(Texture[F32, Texture3d], Sampler, Vector[F32, 3], F32) => Vector[F32, 4],
    [].(Texture[F32, TextureCube], Sampler, Vector[F32, 3], F32) => Vector[F32, 4],

    [].(Texture[F32, Texture3d], Sampler, Vector[F32, 3], F32, Vector[I32, 3]) => Vector[F32, 4],

    [T < ConcreteInteger].(Texture[F32, TextureCubeArray], Sampler, Vector[F32, 3], T, F32) => Vector[F32, 4],

    # L is i32, or u32
    # @must_use fn textureSampleLevel(t: texture_depth_2d, s: sampler, coords: vec2<f32>, level: L) -> f32
    [T < ConcreteInteger].(texture_depth_2d, Sampler, Vector[F32, 2], T) => F32,

    # L is i32, or u32
    # @must_use fn textureSampleLevel(t: texture_depth_2d, s: sampler, coords: vec2<f32>, level: L, offset: vec2<i32>) -> f32
    [T < ConcreteInteger].(texture_depth_2d, Sampler, Vector[F32, 2], T, Vector[I32, 2]) => F32,

    # A is i32, or u32
    # L is i32, or u32
    # @must_use fn textureSampleLevel(t: texture_depth_2d_array, s: sampler, coords: vec2<f32>, array_index: A, level: L) -> f32
    [S < ConcreteInteger, T < ConcreteInteger].(texture_depth_2d_array, Sampler, Vector[F32, 2], S, T) => F32,

    # A is i32, or u32
    # L is i32, or u32
    # @must_use fn textureSampleLevel(t: texture_depth_2d_array, s: sampler, coords: vec2<f32>, array_index: A, level: L, offset: vec2<i32>) -> f32
    [S < ConcreteInteger, T < ConcreteInteger].(texture_depth_2d_array, Sampler, Vector[F32, 2], S, T, Vector[I32, 2]) => F32,

    # L is i32, or u32
    # @must_use fn textureSampleLevel(t: texture_depth_cube, s: sampler, coords: vec3<f32>, level: L) -> f32
    [T < ConcreteInteger].(texture_depth_cube, Sampler, Vector[F32, 3], T) => F32,

    # A is i32, or u32
    # L is i32, or u32
    # @must_use fn textureSampleLevel(t: texture_depth_cube_array, s: sampler, coords: vec3<f32>, array_index: A, level: L) -> f32
    [S < ConcreteInteger, T < ConcreteInteger].(texture_depth_cube_array, Sampler, Vector[F32, 3], S, T) => F32,
}

# 16.7.14
operator :textureSampleBaseClampToEdge, {
    [].(TextureExternal, Sampler, Vector[F32, 2]) => Vector[F32, 4],
    [].(Texture[F32, Texture2d], Sampler, Vector[F32, 2]) => Vector[F32, 4],
}

# 16.7.15 textureStore
# FIXME: this only applies to texture_storage, implement
