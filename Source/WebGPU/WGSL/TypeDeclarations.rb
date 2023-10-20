# FIXME: add all the missing type declarations here

suffixes = {
    f: f32,
    i: i32,
    u: u32,
}

[2, 3, 4].each do |n|
    suffixes.each do |suffix, type|
        type_alias :"vec#{n}#{suffix}", vec[n][type]
    end

    [2, 3, 4].each do |m|
        # FIXME: add alias for F16 once we support it
        #type_alias :"mat#{n}x#{m}f", mat[n][m](f32)[f32, n, m]
        type_alias :"mat#{n}x#{m}f", mat[n,m][f32]
    end
end

operator :+, {
    [T < Number].(T, T) => T,

    # vector addition
    [T < Number, N].(vec[N][T], T) => vec[N][T],
    [T < Number, N].(T, vec[N][T]) => vec[N][T],
    [T < Number, N].(vec[N][T], vec[N][T]) => vec[N][T],

    # matrix-matrix addition
    [T < Float, C, R].(mat[C,R][T], mat[C,R][T]) => mat[C,R][T],
}

operator :-, {
    # unary
    [T < SignedNumber].(T) => T,
    [T < SignedNumber, N].(vec[N][T]) => vec[N][T],

    # binary
    [T < Number].(T, T) => T,
    [T < Number, N].(vec[N][T], T) => vec[N][T],
    [T < Number, N].(T, vec[N][T]) => vec[N][T],
    [T < Number, N].(vec[N][T], vec[N][T]) => vec[N][T],
}

operator :*, {
    # unary
    [AS, T, AM].(ptr[AS, T, AM]) => ref[AS, T, AM],

    # binary
    [T < Number].(T, T) => T,

    # vector scaling
    [T < Number, N].(vec[N][T], T) => vec[N][T],
    [T < Number, N].(T, vec[N][T]) => vec[N][T],
    [T < Number, N].(vec[N][T], vec[N][T]) => vec[N][T],

    # matrix-vector multiplication
    [T < Float, C, R].(mat[C,R][T], vec[C][T]) => vec[R][T],
    [T < Float, C, R].(vec[R][T], mat[C,R][T]) => vec[C][T],

    # matrix-matrix multiplication
    [T < Float, C, R, K].(mat[K,R][T], mat[C,K][T]) => mat[C,R][T],
}

operator :/, {
    [T < Number].(T, T) => T,

    # vector scaling
    [T < Number, N].(vec[N][T], T) => vec[N][T],
    [T < Number, N].(T, vec[N][T]) => vec[N][T],
    [T < Number, N].(vec[N][T], vec[N][T]) => vec[N][T],
}

operator :%, {
    [T < Number].(T, T) => T,

    # vector scaling
    [T < Number, N].(vec[N][T], T) => vec[N][T],
    [T < Number, N].(T, vec[N][T]) => vec[N][T],
    [T < Number, N].(vec[N][T], vec[N][T]) => vec[N][T],
}

# Comparison operations
["==", "!="].each do |op|
    operator :"#{op}", {
        [T < Scalar].(T, T) => bool,
        [T < Scalar, N].(vec[N][T], vec[N][T]) => vec[N][bool],
    }
end

["<", "<=", ">", ">="].each do |op|
    operator :"#{op}", {
        [T < Number].(T, T) => bool,
        [T < Number, N].(vec[N][T], vec[N][T]) => vec[N][bool],
    }
end

# 8.6. Logical Expressions (https://gpuweb.github.io/gpuweb/wgsl/#logical-expr)

operator :!, {
    [].(bool) => bool,
    [N].(vec[N][bool]) => vec[N][bool],
}

operator :'||', {
    [].(bool, bool) => bool,
}

operator :'&&', {
    [].(bool, bool) => bool,
}

operator :'|', {
    [].(bool, bool) => bool,
    [N].(vec[N][bool], vec[N][bool]) => vec[N][bool],
}

operator :'&', {
    # unary
    [AS, T, AM].(ref[AS, T, AM]) => ptr[AS, T, AM],

    # binary
    [].(bool, bool) => bool,
    [N].(vec[N][bool], vec[N][bool]) => vec[N][bool],
}

# 8.9. Bit Expressions (https://www.w3.org/TR/WGSL/#bit-expr)

operator :~, {
    [T < Integer].(T) => T,
    [T < Integer, N].(vec[N][T]) => vec[N][T]
}

["|", "&", "^", "<<", ">>"].each do |op|
    operator :"#{op}", {
        [T < Integer].(T, T) => T,
        [T < Integer, N].(vec[N][T], vec[N][T]) => vec[N][T]
    }
end

# 16.1. Constructor Built-in Functions

# 16.1.1. Zero Value Built-in Functions
operator :bool, {
    [].() => bool,
}
operator :i32, {
    [].() => i32,
}
operator :u32, {
    [].() => u32,
}
operator :f32, {
    [].() => f32,
}
operator :vec2, {
    [T < ConcreteScalar].() => vec2[T],
}
operator :vec3, {
    [T < ConcreteScalar].() => vec3[T],
}
operator :vec4, {
    [T < ConcreteScalar].() => vec4[T],
}
(2..4).each do |columns|
    (2..4).each do |rows|
        operator :"mat#{columns}x#{rows}", {
            [T < ConcreteFloat].() => mat[columns,rows][T],
        }
    end
end

# 16.1.2. Value Constructor Built-in Functions

# 16.1.2.1. array
# Implemented inline in the type checker

# 16.1.2.2.
operator :bool, {
    [T < ConcreteScalar].(T) => bool,
}

# 16.1.2.3. f16
# FIXME: add support for f16

# 16.1.2.4.
operator :f32, {
    [T < ConcreteScalar].(T) => f32,
}

# 16.1.2.5.
operator :i32, {
    [T < ConcreteScalar].(T) => i32,
}

# 16.1.2.6 - 14: matCxR
(2..4).each do |columns|
    (2..4).each do |rows|
        operator :"mat#{columns}x#{rows}", {
            [T < ConcreteFloat, S < Float].(mat[columns,rows][S]) => mat[columns,rows][T],
            [T < Float].(mat[columns,rows][T]) => mat[columns,rows][T],
            [T < Float].(*([T] * columns * rows)) => mat[columns,rows][T],
            [T < Float].(*([vec[rows][T]] * columns)) => mat[columns,rows][T],
        }
    end
end

# 16.1.2.15. Structures
# Implemented inline in the type checker

# 16.1.2.16.
operator :u32, {
    [T < ConcreteScalar].(T) => u32,
}

# 16.1.2.17.
operator :vec2, {
    [T < Scalar].(T) => vec2[T],
    [T < ConcreteScalar, S < Scalar].(vec2[S]) => vec2[T],
    [S < Scalar].(vec2[S]) => vec2[S],
    [T < Scalar].(T, T) => vec2[T],
    [].() => vec2[abstract_int],
}

# 16.1.2.18.
operator :vec3, {
    [T < Scalar].(T) => vec3[T],
    [T < ConcreteScalar, S < Scalar].(vec3[S]) => vec3[T],
    [S < Scalar].(vec3[S]) => vec3[S],
    [T < Scalar].(T, T, T) => vec3[T],
    [T < Scalar].(vec2[T], T) => vec3[T],
    [T < Scalar].(T, vec2[T]) => vec3[T],
    [].() => vec3[abstract_int],
}

# 16.1.2.19.
operator :vec4, {
    [T < Scalar].(T) => vec4[T],
    [T < ConcreteScalar, S < Scalar].(vec4[S]) => vec4[T],
    [S < Scalar].(vec4[S]) => vec4[S],
    [T < Scalar].(T, T, T, T) => vec4[T],
    [T < Scalar].(T, vec2[T], T) => vec4[T],
    [T < Scalar].(T, T, vec2[T]) => vec4[T],
    [T < Scalar].(vec2[T], T, T) => vec4[T],
    [T < Scalar].(vec2[T], vec2[T]) => vec4[T],
    [T < Scalar].(vec3[T], T) => vec4[T],
    [T < Scalar].(T, vec3[T]) => vec4[T],
    [].() => vec4[abstract_int],
}

# 16.2. Bit Reinterpretation Built-in Functions (https://www.w3.org/TR/WGSL/#bitcast-builtin)

# NOTE: Our overload resolution/constraints aren't expressive enough to support
# some of the bitcast overloads. They require a constraint on the type variable
# to constrain it to a vector type, which we cannot express here, so it's implemented
# inline in the type checker

# 17.3. Logical Built-in Functions (https://www.w3.org/TR/WGSL/#logical-builtin-functions)

# 17.3.1 & 17.3.2
["all", "any"].each do |op|
    operator :"#{op}", {
        [N].(vec[N][bool]) => bool,
        [N].(bool) => bool,
    }
end

# 17.3.3
operator :select, {
    [T < Scalar].(T, T, bool) => T,
    [T < Scalar, N].(vec[N][T], vec[N][T], bool) => vec[N][T],
    [T < Scalar, N].(vec[N][T], vec[N][T], vec[N][bool]) => vec[N][T],
}

# 16.4. Array Built-in Functions

# 16.4.1.
operator :arrayLength, {
  [T].(ptr[storage, array[T], read]) => u32,
  [T].(ptr[storage, array[T], read_write]) => u32,
}

# 17.5. Numeric Built-in Functions (https://www.w3.org/TR/WGSL/#numeric-builtin-functions)

# Trigonometric
["acos", "asin", "atan", "cos", "sin", "tan",
 "acosh", "asinh", "atanh", "cosh", "sinh", "tanh"].each do |op|
    operator :"#{op}", {
        [T < Float].(T) => T,
        [T < Float, N].(vec[N][T]) => vec[N][T],
    }
end

# 17.5.1
operator :abs, {
    [T < Number].(T) => T,
    [T < Number, N].(vec[N][T]) => vec[N][T],
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
    [T < Float, N].(vec[N][T], vec[N][T]) => vec[N][T],
}

# 17.5.9
operator :ceil, {
    [T < Float].(T) => T,
    [T < Float, N].(vec[N][T]) => vec[N][T],
}

# 17.5.10
operator :clamp, {
    [T < Number].(T, T, T) => T,
    [T < Number, N].(vec[N][T], vec[N][T], vec[N][T]) => vec[N][T],
}

# 17.5.11. cos
# 17.5.12. cosh
# Defined in [Trigonometric]

# 17.5.13-15 (Bit counting)
["countLeadingZeros", "countOneBits", "countTrailingZeros"].each do |op|
    operator :"#{op}", {
        [T < ConcreteInteger].(T) => T,
        [T < ConcreteInteger, N].(vec[N][T]) => vec[N][T],
    }
end

# 17.5.16
operator :cross, {
    [T < Float].(vec3[T], vec3[T]) => vec3[T],
}

# 17.5.17
operator :degrees, {
    [T < Float].(T) => T,
    [T < Float, N].(vec[N][T]) => vec[N][T],
}

# 17.5.18
operator :determinant, {
    [T < Float, C].(mat[C,C][T]) => T,
}

# 17.5.19
operator :distance, {
    [T < Float].(T, T) => T,
    [T < Float, N].(vec[N][T], vec[N][T]) => T,
}

# 17.5.20
operator :dot, {
    [T < Number, N].(vec[N][T], vec[N][T]) => T
}

# 17.5.21 & 17.5.22
["exp", "exp2"].each do |op|
    operator :"#{op}", {
        [T < Float].(T) => T,
        [T < Float, N].(vec[N][T]) => vec[N][T],
    }
end

# 17.5.23 & 17.5.24
operator :extractBits, {
    # signed
    [].(i32, u32, u32) => i32,
    [N].(vec[N][i32], u32, u32) => vec[N][i32],

    # unsigned
    [].(u32, u32, u32) => u32,
    [N].(vec[N][u32], u32, u32) => vec[N][u32],
}

# 17.5.25
operator :faceForward, {
    [T < Float, N].(vec[N][T], vec[N][T], vec[N][T]) => vec[N][T],
}

# 17.5.26 & 17.5.27
operator :firstLeadingBit, {
    # signed
    [].(i32) => i32,
    [N].(vec[N][i32]) => vec[N][i32],

    # unsigned
    [].(u32) => u32,
    [N].(vec[N][u32]) => vec[N][u32],
}

# 17.5.28
operator :firstTrailingBit, {
    [T < ConcreteInteger].(T) => T,
    [T < ConcreteInteger, N].(vec[N][T]) => vec[N][T],
}

# 17.5.29
operator :floor, {
    [T < Float].(T) => T,
    [T < Float, N].(vec[N][T]) => vec[N][T],
}

# 17.5.30
operator :fma, {
    [T < Float].(T, T, T) => T,
    [T < Float, N].(vec[N][T], vec[N][T], vec[N][T]) => vec[N][T],
}

# 17.5.31
operator :fract, {
    [T < Float].(T) => T,
    [T < Float, N].(vec[N][T]) => vec[N][T],
}

# 17.5.32
operator :frexp, {
    # FIXME: this needs the special return types __frexp_result_*
}

# 17.5.33
operator :insertBits, {
    [T < ConcreteInteger].(T, T, u32, u32) => T,
    [T < ConcreteInteger, N].(vec[N][T], vec[N][T], u32, u32) => vec[N][T],
}

# 17.5.34
operator :inverseSqrt, {
    [T < Float].(T) => T,
    [T < Float, N].(vec[N][T]) => vec[N][T],
}

# 17.5.35
operator :ldexp, {
    [T < ConcreteFloat].(T, i32) => T,
    [].(abstract_float, abstract_int) => abstract_float,
    [T < ConcreteFloat, N].(vec[N][T], vec[N][i32]) => vec[N][T],
    [N].(vec[N][abstract_float], vec[N][abstract_int]) => vec[N][abstract_float],
}

# 17.5.36
operator :length, {
    [T < Float].(T) => T,
    [T < Float, N].(vec[N][T]) => T,
}

# 17.5.37 & 17.5.38
["log", "log2"].each do |op|
    operator :"#{op}", {
        [T < Float].(T) => T,
        [T < Float, N].(vec[N][T]) => vec[N][T],
    }
end

# 17.5.39
operator :max, {
    [T < Number].(T, T) => T,
    [T < Number, N].(vec[N][T], vec[N][T]) => vec[N][T],
}

# 17.5.40
operator :min, {
    [T < Number].(T, T) => T,
    [T < Number, N].(vec[N][T], vec[N][T]) => vec[N][T],
}

# 17.5.41
operator :mix, {
    [T < Float].(T, T, T) => T,
    [T < Float, N].(vec[N][T], vec[N][T], vec[N][T]) => vec[N][T],
    [T < Float, N].(vec[N][T], vec[N][T], T) => vec[N][T],
}

# 17.5.42
operator :modf, {
    # FIXME: this needs the special return types __modf_result_*
}

# 17.5.43
operator :normalize, {
    [T < Float, N].(vec[N][T]) => vec[N][T],
}

# 17.5.44
operator :pow, {
    [T < Float].(T, T) => T,
    [T < Float, N].(vec[N][T], vec[N][T]) => vec[N][T],
}

# 17.5.45
operator :quantizeToF16, {
    [].(f32) => f32,
    [N].(vec[N][f32]) => vec[N][f32],
}

# 17.5.46
operator :radians, {
    [T < Float].(T) => T,
    [T < Float, N].(vec[N][T]) => vec[N][T],
}

# 17.5.47
operator :reflect, {
    [T < Float, N].(vec[N][T], vec[N][T]) => vec[N][T],
}

# 17.5.48
operator :refract, {
    [T < Float, N].(vec[N][T], vec[N][T], T) => vec[N][T],
}

# 17.5.49
operator :reverseBits, {
    [T < ConcreteInteger].(T) => T,
    [T < ConcreteInteger, N].(vec[N][T]) => vec[N][T],
}

# 17.5.50
operator :round, {
    [T < Float].(T) => T,
    [T < Float, N].(vec[N][T]) => vec[N][T],
}

# 17.5.51
operator :saturate, {
    [T < Float].(T) => T,
    [T < Float, N].(vec[N][T]) => vec[N][T],
}

# 17.5.52
operator :sign, {
    [T < SignedNumber].(T) => T,
    [T < SignedNumber, N].(vec[N][T]) => vec[N][T],
}

# 17.5.53. sin
# 17.5.54. sinh
# Defined in [Trigonometric]

# 17.5.55
operator :smoothstep, {
    [T < Float].(T, T, T) => T,
    [T < Float, N].(vec[N][T], vec[N][T], vec[N][T]) => vec[N][T],
}

# 17.5.56
operator :sqrt, {
    [T < Float].(T) => T,
    [T < Float, N].(vec[N][T]) => vec[N][T],
}

# 17.5.57
operator :step, {
    [T < Float].(T, T) => T,
    [T < Float, N].(vec[N][T], vec[N][T]) => vec[N][T],
}

# 17.5.58. tan
# 17.5.59. tanh
# Defined in [Trigonometric]

# 17.5.60
operator :transpose, {
    [T < Float, C, R].(mat[C,R][T]) => mat[R,C][T],
}

# 17.5.61
operator :trunc, {
    [T < Float].(T) => T,
    [T < Float, N].(vec[N][T]) => vec[N][T],
}

# 16.7. Texture Built-in Functions (https://gpuweb.github.io/gpuweb/wgsl/#texture-builtin-functions)

# 16.7.1
operator :textureDimensions, {
    # ST is i32, u32, or f32
    # F is a texel format
    # A is an access mode
    # T is texture_1d<ST> or texture_storage_1d<F,A>
    # @must_use fn textureDimensions(t: T) -> u32
    [S < Concrete32BitNumber].(texture_1d[S]) => u32,
    [F, AM].(texture_storage_1d[F, AM]) => u32,

    # ST is i32, u32, or f32
    # T is texture_1d<ST>
    # L is i32, or u32
    # @must_use fn textureDimensions(t: T, level: L) -> u32
    [S < Concrete32BitNumber, T < ConcreteInteger].(texture_1d[S], T) => u32,

    # ST is i32, u32, or f32
    # F is a texel format
    # A is an access mode
    # T is texture_2d<ST>, texture_2d_array<ST>, texture_cube<ST>, texture_cube_array<ST>, texture_multisampled_2d<ST>, texture_depth_2d, texture_depth_2d_array, texture_depth_cube, texture_depth_cube_array, texture_depth_multisampled_2d, texture_storage_2d<F,A>, texture_storage_2d_array<F,A>, or texture_external
    # @must_use fn textureDimensions(t: T) -> vec2<u32>
    [S < Concrete32BitNumber].(texture_2d[S]) => vec2[u32],
    [S < Concrete32BitNumber].(texture_2d_array[S]) => vec2[u32],
    [S < Concrete32BitNumber].(texture_cube[S]) => vec2[u32],
    [S < Concrete32BitNumber].(texture_cube_array[S]) => vec2[u32],
    [S < Concrete32BitNumber].(texture_multisampled_2d[S]) => vec2[u32],
    [].(texture_depth_2d) => vec2[u32],
    [].(texture_depth_2d_array) => vec2[u32],
    [].(texture_depth_cube) => vec2[u32],
    [].(texture_depth_cube_array) => vec2[u32],
    [].(texture_depth_multisampled_2d) => vec2[u32],
    [F, AM].(texture_storage_2d[F, AM]) => vec2[u32],
    [F, AM].(texture_storage_2d_array[F, AM]) => vec2[u32],
    [].(texture_external) => vec2[u32],

    # ST is i32, u32, or f32
    # T is texture_2d<ST>, texture_2d_array<ST>, texture_cube<ST>, texture_cube_array<ST>, texture_depth_2d, texture_depth_2d_array, texture_depth_cube, or texture_depth_cube_array
    # L is i32, or u32
    # @must_use fn textureDimensions(t: T, level: L) -> vec2<u32>
    [S < Concrete32BitNumber, T < ConcreteInteger].(texture_2d[S], T) => vec2[u32],
    [S < Concrete32BitNumber, T < ConcreteInteger].(texture_2d_array[S], T) => vec2[u32],
    [S < Concrete32BitNumber, T < ConcreteInteger].(texture_cube[S], T) => vec2[u32],
    [S < Concrete32BitNumber, T < ConcreteInteger].(texture_cube_array[S], T) => vec2[u32],
    [T < ConcreteInteger].(texture_depth_2d, T) => vec2[u32],
    [T < ConcreteInteger].(texture_depth_2d_array, T) => vec2[u32],
    [T < ConcreteInteger].(texture_depth_cube, T) => vec2[u32],
    [T < ConcreteInteger].(texture_depth_cube_array, T) => vec2[u32],

    # ST is i32, u32, or f32
    # F is a texel format
    # A is an access mode
    # T is texture_3d<ST> or texture_storage_3d<F,A>
    # @must_use fn textureDimensions(t: T) -> vec3<u32>
    [S < Concrete32BitNumber].(texture_3d[S]) => vec3[u32],
    [F, AM].(texture_storage_3d[F, AM]) => vec3[u32],

    # ST is i32, u32, or f32
    # T is texture_3d<ST>
    # L is i32, or u32
    # @must_use fn textureDimensions(t: T, level: L) -> vec3<u32>
    [S < Concrete32BitNumber, T < ConcreteInteger].(texture_3d[S], T) => vec3[u32],
}

# 16.7.2
operator :textureGather, {
    [T < ConcreteInteger, S < Concrete32BitNumber].(T, texture_2d[S], sampler, vec2[f32]) => vec4[S],

    [T < ConcreteInteger, S < Concrete32BitNumber].(T, texture_2d[S], sampler, vec2[f32], vec2[i32]) => vec4[S],

    [T < ConcreteInteger, S < Concrete32BitNumber, U < ConcreteInteger].(T, texture_2d_array[S], sampler, vec2[f32], U) => vec4[S],

    [T < ConcreteInteger, S < Concrete32BitNumber, U < ConcreteInteger].(T, texture_2d_array[S], sampler, vec2[f32], U, vec2[i32]) => vec4[S],

    [T < ConcreteInteger, S < Concrete32BitNumber].(T, texture_cube[S], sampler, vec3[f32]) => vec4[S],

    [T < ConcreteInteger, S < Concrete32BitNumber, U < ConcreteInteger].(T, texture_cube_array[S], sampler, vec3[f32], U) => vec4[S],

    # @must_use fn textureGather(t: texture_depth_2d, s: sampler, coords: vec2<f32>) -> vec4<f32>
    [].(texture_depth_2d, sampler, vec2[f32]) => vec4[f32],

    # @must_use fn textureGather(t: texture_depth_2d, s: sampler, coords: vec2<f32>, offset: vec2<i32>) -> vec4<f32>
    [].(texture_depth_2d, sampler, vec2[f32], vec2[i32]) => vec4[f32],

    # @must_use fn textureGather(t: texture_depth_cube, s: sampler, coords: vec3<f32>) -> vec4<f32>
    [].(texture_depth_cube, sampler, vec3[f32]) => vec4[f32],

    # A is i32, or u32
    # @must_use fn textureGather(t: texture_depth_2d_array, s: sampler, coords: vec2<f32>, array_index: A) -> vec4<f32>
    [U < ConcreteInteger].(texture_depth_2d_array, sampler, vec2[f32], U) => vec4[f32],

    # A is i32, or u32
    # @must_use fn textureGather(t: texture_depth_2d_array, s: sampler, coords: vec2<f32>, array_index: A, offset: vec2<i32>) -> vec4<f32>
    [U < ConcreteInteger].(texture_depth_2d_array, sampler, vec2[f32], U, vec2[i32]) => vec4[f32],

    # A is i32, or u32
    # @must_use fn textureGather(t: texture_depth_cube_array, s: sampler, coords: vec3<f32>, array_index: A) -> vec4<f32>
    [U < ConcreteInteger].(texture_depth_cube_array, sampler, vec3[f32], U) => vec4[f32],
}

# 16.7.3
operator :textureGatherCompare, {
    # @must_use fn textureGatherCompare(t: texture_depth_2d, s: sampler_comparison, coords: vec2<f32>, depth_ref: f32) -> vec4<f32>
    [].(texture_depth_2d, sampler_comparison, vec2[f32], f32) => vec4[f32],

    # @must_use fn textureGatherCompare(t: texture_depth_2d, s: sampler_comparison, coords: vec2<f32>, depth_ref: f32, offset: vec2<i32>) -> vec4<f32>
    [].(texture_depth_2d, sampler_comparison, vec2[f32], f32, vec2[i32]) => vec4[f32],

    # A is i32, or u32
    # @must_use fn textureGatherCompare(t: texture_depth_2d_array, s: sampler_comparison, coords: vec2<f32>, array_index: A, depth_ref: f32) -> vec4<f32>
    [T < ConcreteInteger].(texture_depth_2d_array, sampler_comparison, vec2[f32], T, f32) => vec4[f32],

    # A is i32, or u32
    # @must_use fn textureGatherCompare(t: texture_depth_2d_array, s: sampler_comparison, coords: vec2<f32>, array_index: A, depth_ref: f32, offset: vec2<i32>) -> vec4<f32>
    [T < ConcreteInteger].(texture_depth_2d_array, sampler_comparison, vec2[f32], T, f32, vec2[i32]) => vec4[f32],

    # @must_use fn textureGatherCompare(t: texture_depth_cube, s: sampler_comparison, coords: vec3<f32>, depth_ref: f32) -> vec4<f32>
    [].(texture_depth_cube, sampler_comparison, vec3[f32], f32) => vec4[f32],

    # A is i32, or u32
    # @must_use fn textureGatherCompare(t: texture_depth_cube_array, s: sampler_comparison, coords: vec3<f32>, array_index: A, depth_ref: f32) -> vec4<f32>
    [T < ConcreteInteger].(texture_depth_cube_array, sampler_comparison, vec3[f32], T, f32) => vec4[f32],
}

# 16.7.4
operator :textureLoad, {
    [T < ConcreteInteger, U < ConcreteInteger, S < Concrete32BitNumber].(texture_1d[S], T, U) => vec4[S],
    [T < ConcreteInteger, U < ConcreteInteger, S < Concrete32BitNumber].(texture_2d[S], vec2[T], U) => vec4[S],
    [T < ConcreteInteger, V < ConcreteInteger, U < ConcreteInteger, S < Concrete32BitNumber].(texture_2d_array[S], vec2[T], V, U) => vec4[S],
    [T < ConcreteInteger, U < ConcreteInteger, S < Concrete32BitNumber].(texture_3d[S], vec3[T], U) => vec4[S],
    [T < ConcreteInteger, U < ConcreteInteger, S < Concrete32BitNumber].(texture_multisampled_2d[S], vec2[T], U) => vec4[S],


    # C is i32, or u32
    # L is i32, or u32
    # @must_use fn textureLoad(t: texture_depth_2d, coords: vec2<C>, level: L) -> f32
    [T < ConcreteInteger, U < ConcreteInteger].(texture_depth_2d, vec2[T], U) => f32,

    # C is i32, or u32
    # A is i32, or u32
    # L is i32, or u32
    # @must_use fn textureLoad(t: texture_depth_2d_array, coords: vec2<C>, array_index: A, level: L) -> f32
    [T < ConcreteInteger, S < ConcreteInteger, U < ConcreteInteger].(texture_depth_2d_array, vec2[T], S, U) => f32,

    # C is i32, or u32
    # S is i32, or u32
    # @must_use fn textureLoad(t: texture_depth_multisampled_2d, coords: vec2<C>, sample_index: S)-> f32
    [T < ConcreteInteger, U < ConcreteInteger].(texture_depth_multisampled_2d, vec2[T], U) => f32,

    [T < ConcreteInteger].(texture_external, vec2[T]) => vec4[f32],
}

# 16.7.5
operator :textureNumLayers, {
    # F is a texel format
    # A is an access mode
    # ST is i32, u32, or f32
    # T is texture_2d_array<ST>, texture_cube_array<ST>, texture_depth_2d_array, texture_depth_cube_array, or texture_storage_2d_array<F,A>
    # @must_use fn textureNumLayers(t: T) -> u32
    [S < Concrete32BitNumber].(texture_2d_array[S]) => u32,
    [S < Concrete32BitNumber].(texture_cube_array[S]) => u32,
    [].(texture_depth_2d_array) => u32,
    [].(texture_depth_cube_array) => u32,
    [F, AM].(texture_storage_2d_array[F, AM]) => u32,
}

# 16.7.6
operator :textureNumLevels, {
    # ST is i32, u32, or f32
    # T is texture_1d<ST>, texture_2d<ST>, texture_2d_array<ST>, texture_3d<ST>, texture_cube<ST>, texture_cube_array<ST>, texture_depth_2d, texture_depth_2d_array, texture_depth_cube, or texture_depth_cube_array
    # @must_use fn textureNumLevels(t: T) -> u32
    [S < Concrete32BitNumber].(texture_1d[S]) => u32,
    [S < Concrete32BitNumber].(texture_2d[S]) => u32,
    [S < Concrete32BitNumber].(texture_2d_array[S]) => u32,
    [S < Concrete32BitNumber].(texture_3d[S]) => u32,
    [S < Concrete32BitNumber].(texture_cube[S]) => u32,
    [S < Concrete32BitNumber].(texture_cube_array[S]) => u32,
    [].(texture_depth_2d) => u32,
    [].(texture_depth_2d_array) => u32,
    [].(texture_depth_cube) => u32,
    [].(texture_depth_cube_array) => u32,
}

# 16.7.7
operator :textureNumSamples, {
    # ST is i32, u32, or f32
    # T is texture_multisampled_2d<ST> or texture_depth_multisampled_2d
    # @must_use fn textureNumSamples(t: T) -> u32
    [S < Concrete32BitNumber].(texture_multisampled_2d[S]) => u32,
    [].(texture_depth_multisampled_2d) => u32,
}

# 16.7.8
operator :textureSample, {
    [].(texture_1d[f32], sampler, f32) => vec4[f32],

    [].(texture_2d[f32], sampler, vec2[f32]) => vec4[f32],

    [].(texture_2d[f32], sampler, vec2[f32], vec2[i32]) => vec4[f32],

    [T < ConcreteInteger].(texture_2d_array[f32], sampler, vec2[f32], T) => vec4[f32],

    [T < ConcreteInteger].(texture_2d_array[f32], sampler, vec2[f32], T, vec2[i32]) => vec4[f32],

    [].(texture_3d[f32], sampler, vec3[f32]) => vec4[f32],
    [].(texture_cube[f32], sampler, vec3[f32]) => vec4[f32],

    [].(texture_3d[f32], sampler, vec3[f32], vec3[i32]) => vec4[f32],

    [T < ConcreteInteger].(texture_cube_array[f32], sampler, vec3[f32], T) => vec4[f32],

    # @must_use fn textureSample(t: texture_depth_2d, s: sampler, coords: vec2<f32>) -> f32
    [].(texture_depth_2d, sampler, vec2[f32]) => f32,

    # @must_use fn textureSample(t: texture_depth_2d, s: sampler, coords: vec2<f32>, offset: vec2<i32>) -> f32
    [].(texture_depth_2d, sampler, vec2[f32], vec2[i32]) => f32,

    # A is i32, or u32
    # @must_use fn textureSample(t: texture_depth_2d_array, s: sampler, coords: vec2<f32>, array_index: A) -> f32
    [T < ConcreteInteger].(texture_depth_2d_array, sampler, vec2[f32], T) => f32,

    # A is i32, or u32
    # @must_use fn textureSample(t: texture_depth_2d_array, s: sampler, coords: vec2<f32>, array_index: A, offset: vec2<i32>) -> f32
    [T < ConcreteInteger].(texture_depth_2d_array, sampler, vec2[f32], T, vec2[i32]) => f32,

    # @must_use fn textureSample(t: texture_depth_cube, s: sampler, coords: vec3<f32>) -> f32
    [].(texture_depth_cube, sampler, vec3[f32]) => f32,

    # A is i32, or u32
    # @must_use fn textureSample(t: texture_depth_cube_array, s: sampler, coords: vec3<f32>, array_index: A) -> f32
    [T < ConcreteInteger].(texture_depth_cube_array, sampler, vec3[f32], T) => f32,
}

# 16.7.9
operator :textureSampleBias, {
    [].(texture_2d[f32], sampler, vec2[f32], f32) => vec4[f32],

    [].(texture_2d[f32], sampler, vec2[f32], f32, vec2[i32]) => vec4[f32],

    [T < ConcreteInteger].(texture_2d_array[f32], sampler, vec2[f32], T, f32) => vec4[f32],

    [T < ConcreteInteger].(texture_2d_array[f32], sampler, vec2[f32], T, f32, vec2[i32]) => vec4[f32],

    [].(texture_3d[f32], sampler, vec3[f32], f32) => vec4[f32],
    [].(texture_cube[f32], sampler, vec3[f32], f32) => vec4[f32],

    [].(texture_3d[f32], sampler, vec3[f32], f32, vec3[i32]) => vec4[f32],

    [T < ConcreteInteger].(texture_cube_array[f32], sampler, vec3[f32], T, f32) => vec4[f32],
}

# 16.7.10
operator :textureSampleCompare, {
    # @must_use fn textureSampleCompare(t: texture_depth_2d, s: sampler_comparison, coords: vec2<f32>, depth_ref: f32) -> f32
    [].(texture_depth_2d, sampler_comparison, vec2[f32], f32) => f32,

    # @must_use fn textureSampleCompare(t: texture_depth_2d, s: sampler_comparison, coords: vec2<f32>, depth_ref: f32, offset: vec2<i32>) -> f32
    [].(texture_depth_2d, sampler_comparison, vec2[f32], f32, vec2[i32]) => f32,

    # A is i32, or u32
    # @must_use fn textureSampleCompare(t: texture_depth_2d_array, s: sampler_comparison, coords: vec2<f32>, array_index: A, depth_ref: f32) -> f32
    [T < ConcreteInteger].(texture_depth_2d_array, sampler_comparison, vec2[f32], T, f32) => f32,

    # A is i32, or u32
    # @must_use fn textureSampleCompare(t: texture_depth_2d_array, s: sampler_comparison, coords: vec2<f32>, array_index: A, depth_ref: f32, offset: vec2<i32>) -> f32
    [T < ConcreteInteger].(texture_depth_2d_array, sampler_comparison, vec2[f32], T, f32, vec2[i32]) => f32,

    # @must_use fn textureSampleCompare(t: texture_depth_cube, s: sampler_comparison, coords: vec3<f32>, depth_ref: f32) -> f32
    [].(texture_depth_cube, sampler_comparison, vec3[f32], f32) => f32,

    # A is i32, or u32
    # @must_use fn textureSampleCompare(t: texture_depth_cube_array, s: sampler_comparison, coords: vec3<f32>, array_index: A, depth_ref: f32) -> f32
    [T < ConcreteInteger].(texture_depth_cube_array, sampler_comparison, vec3[f32], T, f32) => f32,
}

# 16.7.11
operator :textureSampleCompareLevel, {
    # @must_use fn textureSampleCompareLevel(t: texture_depth_2d, s: sampler_comparison, coords: vec2<f32>, depth_ref: f32) -> f32
    [].(texture_depth_2d, sampler_comparison, vec2[f32], f32) => f32,

    # @must_use fn textureSampleCompareLevel(t: texture_depth_2d, s: sampler_comparison, coords: vec2<f32>, depth_ref: f32, offset: vec2<i32>) -> f32
    [].(texture_depth_2d, sampler_comparison, vec2[f32], f32, vec2[i32]) => f32,

    # A is i32, or u32
    # @must_use fn textureSampleCompareLevel(t: texture_depth_2d_array, s: sampler_comparison, coords: vec2<f32>, array_index: A, depth_ref: f32) -> f32
    [T < ConcreteInteger].(texture_depth_2d_array, sampler_comparison, vec2[f32], T, f32) => f32,

    # A is i32, or u32
    # @must_use fn textureSampleCompareLevel(t: texture_depth_2d_array, s: sampler_comparison, coords: vec2<f32>, array_index: A, depth_ref: f32, offset: vec2<i32>) -> f32
    [T < ConcreteInteger].(texture_depth_2d_array, sampler_comparison, vec2[f32], T, f32, vec2[i32]) => f32,

    # @must_use fn textureSampleCompareLevel(t: texture_depth_cube, s: sampler_comparison, coords: vec3<f32>, depth_ref: f32) -> f32
    [].(texture_depth_cube, sampler_comparison, vec3[f32], f32) => f32,

    # A is i32, or u32
    # @must_use fn textureSampleCompareLevel(t: texture_depth_cube_array, s: sampler_comparison, coords: vec3<f32>, array_index: A, depth_ref: f32) -> f32
    [T < ConcreteInteger].(texture_depth_cube_array, sampler_comparison, vec3[f32], T, f32) => f32,
}

# 16.7.12
operator :textureSampleGrad, {
    [].(texture_2d[f32], sampler, vec2[f32], vec2[f32], vec2[f32]) => vec4[f32],

    [].(texture_2d[f32], sampler, vec2[f32], vec2[f32], vec2[f32], vec2[i32]) => vec4[f32],

    [T < ConcreteInteger].(texture_2d_array[f32], sampler, vec2[f32], T, vec2[f32], vec2[f32]) => vec4[f32],

    [T < ConcreteInteger].(texture_2d_array[f32], sampler, vec2[f32], T, vec2[f32], vec2[f32], vec2[i32]) => vec4[f32],

    [].(texture_3d[f32], sampler, vec3[f32], vec3[f32], vec3[f32]) => vec4[f32],
    [].(texture_cube[f32], sampler, vec3[f32], vec3[f32], vec3[f32]) => vec4[f32],

    [].(texture_3d[f32], sampler, vec3[f32], vec3[f32], vec3[f32], vec3[i32]) => vec4[f32],

    [T < ConcreteInteger].(texture_cube_array[f32], sampler, vec3[f32], T, vec3[f32], vec3[f32]) => vec4[f32],
}

# 16.7.13
operator :textureSampleLevel, {
    [].(texture_2d[f32], sampler, vec2[f32], f32) => vec4[f32],

    [].(texture_2d[f32], sampler, vec2[f32], f32, vec2[i32]) => vec4[f32],

    [T < ConcreteInteger].(texture_2d_array[f32], sampler, vec2[f32], T, f32) => vec4[f32],

    [T < ConcreteInteger].(texture_2d_array[f32], sampler, vec2[f32], T, f32, vec2[i32]) => vec4[f32],

    [].(texture_3d[f32], sampler, vec3[f32], f32) => vec4[f32],
    [].(texture_cube[f32], sampler, vec3[f32], f32) => vec4[f32],

    [].(texture_3d[f32], sampler, vec3[f32], f32, vec3[i32]) => vec4[f32],

    [T < ConcreteInteger].(texture_cube_array[f32], sampler, vec3[f32], T, f32) => vec4[f32],

    # L is i32, or u32
    # @must_use fn textureSampleLevel(t: texture_depth_2d, s: sampler, coords: vec2<f32>, level: L) -> f32
    [T < ConcreteInteger].(texture_depth_2d, sampler, vec2[f32], T) => f32,

    # L is i32, or u32
    # @must_use fn textureSampleLevel(t: texture_depth_2d, s: sampler, coords: vec2<f32>, level: L, offset: vec2<i32>) -> f32
    [T < ConcreteInteger].(texture_depth_2d, sampler, vec2[f32], T, vec2[i32]) => f32,

    # A is i32, or u32
    # L is i32, or u32
    # @must_use fn textureSampleLevel(t: texture_depth_2d_array, s: sampler, coords: vec2<f32>, array_index: A, level: L) -> f32
    [S < ConcreteInteger, T < ConcreteInteger].(texture_depth_2d_array, sampler, vec2[f32], S, T) => f32,

    # A is i32, or u32
    # L is i32, or u32
    # @must_use fn textureSampleLevel(t: texture_depth_2d_array, s: sampler, coords: vec2<f32>, array_index: A, level: L, offset: vec2<i32>) -> f32
    [S < ConcreteInteger, T < ConcreteInteger].(texture_depth_2d_array, sampler, vec2[f32], S, T, vec2[i32]) => f32,

    # L is i32, or u32
    # @must_use fn textureSampleLevel(t: texture_depth_cube, s: sampler, coords: vec3<f32>, level: L) -> f32
    [T < ConcreteInteger].(texture_depth_cube, sampler, vec3[f32], T) => f32,

    # A is i32, or u32
    # L is i32, or u32
    # @must_use fn textureSampleLevel(t: texture_depth_cube_array, s: sampler, coords: vec3<f32>, array_index: A, level: L) -> f32
    [S < ConcreteInteger, T < ConcreteInteger].(texture_depth_cube_array, sampler, vec3[f32], S, T) => f32,
}

# 16.7.14
operator :textureSampleBaseClampToEdge, {
    [].(texture_external, sampler, vec2[f32]) => vec4[f32],
    [].(texture_2d[f32], sampler, vec2[f32]) => vec4[f32],
}

# 16.7.15 textureStore
operator :textureStore, {
    # F is a texel format
    # C is i32, or u32
    # CF depends on the storage texel format F. See the texel format table for the mapping of texel format to channel format.
    # fn textureStore(t: texture_storage_1d<F,write>, coords: C, value: vec4<CF>)
    [F, T < ConcreteInteger].(texture_storage_1d[F, write], T, vec4[ChannelFormat[F]]) => void,

    # F is a texel format
    # C is i32, or u32
    # CF depends on the storage texel format F. See the texel format table for the mapping of texel format to channel format.
    # fn textureStore(t: texture_storage_2d<F,write>, coords: vec2<C>, value: vec4<CF>)
    [F, T < ConcreteInteger].(texture_storage_2d[F, write], vec2[T], vec4[ChannelFormat[F]]) => void,

    # F is a texel format
    # C is i32, or u32
    # A is i32, or u32
    # CF depends on the storage texel format F. See the texel format table for the mapping of texel format to channel format.
    # fn textureStore(t: texture_storage_2d_array<F,write>, coords: vec2<C>, array_index: A, value: vec4<CF>)
    [F, T < ConcreteInteger, S < ConcreteInteger].(texture_storage_2d_array[F, write], vec2[T], S, vec4[ChannelFormat[F]]) => void,

    # F is a texel format
    # C is i32, or u32
    # CF depends on the storage texel format F. See the texel format table for the mapping of texel format to channel format.
    # fn textureStore(t: texture_storage_3d<F,write>, coords: vec3<C>, value: vec4<CF>)
    [F, T < ConcreteInteger].(texture_storage_3d[F, write], vec3[T], vec4[ChannelFormat[F]]) => void,
}

# 16.8. Atomic Built-in Functions (https://www.w3.org/TR/WGSL/#atomic-builtin-functions)

# 16.8.1
operator :atomicLoad, {
    # fn atomicLoad(atomic_ptr: ptr<AS, atomic<T>, read_write>) -> T
    [AS, T].(ptr[AS, atomic[T], read_write]) => T,
}


# 16.8.2
operator :atomicStore, {
    # fn atomicStore(atomic_ptr: ptr<AS, atomic<T>, read_write>, v: T)
    [AS, T].(ptr[AS, atomic[T], read_write], T) => void,
}

# 16.8.3. Atomic Read-modify-write (this spec entry contains several functions)
[
    :atomicAdd,
    :atomicSub,
    :atomicMax,
    :atomicMin,
    :atomicOr,
    :atomicXor,
    :atomicExchange,
].each do |op|
    operator op, {
        # fn #{op}(atomic_ptr: ptr<AS, atomic<T>, read_write>, v: T) -> T
        [AS, T].(ptr[AS, atomic[T], read_write], T) => T,
    }
end

# FIXME: Implement atomicCompareExchangeWeak (which depends on the result struct that is not currently supported)
# fn atomicCompareExchangeWeak(atomic_ptr: ptr<AS, atomic<T>, read_write>, cmp: T, v: T) -> __atomic_compare_exchange_result<T>

# 16.11. Synchronization Built-in Functions (https://www.w3.org/TR/WGSL/#sync-builtin-functions)

# 16.11.1.
operator :storageBarrier, {
    # fn storageBarrier()
    [].() => void,
}

# 16.11.2.
operator :workgroupBarrier, {
    # fn workgroupBarrier()
    [].() => void,
}

# 16.11.3.
operator :workgroupUniformLoad, {
    # @must_use fn workgroupUniformLoad(p : ptr<workgroup, T>) -> T
    [T].(ptr[workgroup, T]) => void,
}
