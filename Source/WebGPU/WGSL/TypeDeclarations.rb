# FIXME: add all the missing type declarations here
operator :+, {
    [T < Number].(T, T) => T,
    [T < Number, N].(Vector[T, N], T) => Vector[T, N],
    [T < Number, N].(T, Vector[T, N]) => Vector[T, N],
    [T < Number, N].(Vector[T, N], Vector[T, N]) => Vector[T, N],
    [T < Float, C, R].(Matrix[T, C, R], Matrix[T, C, R]) => Matrix[T, C, R],
}

operator :*, {
    [T < Float, C, R].(Matrix[T, C, R], Vector[T, C]) => Vector[T, R],
    [T < Float, C, R].(Vector[T, R], Matrix[T, C, R]) => Vector[T, C],
}

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

operator :vec2, {
    # FIXME: overload resolution should support explicitly instantiating variables
    # [S < Scalar, T < ConcreteScalar].(Vector[S, 2]) => Vector[T, 2],
    [S < Scalar].(Vector[S, 2]) => Vector[S, 2],
    [T < Scalar].(T, T) => Vector[T, 2],
}

operator :vec3, {
    # FIXME: overload resolution should support explicitly instantiating variables
    # [S < Scalar, T < ConcreteScalar].(Vector[S, 3]) => Vector[T, 3],
    [S < Scalar].(Vector[S, 3]) => Vector[S, 3],
    [T < Scalar].(T, T, T) => Vector[T, 3],
    [T < Scalar].(Vector[T, 2], T) => Vector[T, 3],
    [T < Scalar].(T, Vector[T, 2]) => Vector[T, 3],
}

operator :vec4, {
    # FIXME: overload resolution should support explicitly instantiating variables
    # [S < Scalar, T < ConcreteScalar].(Vector[S, 4]) => Vector[T, 4],
    [S < Scalar].(Vector[S, 4]) => Vector[S, 4],
    [T < Scalar].(T, T, T, T) => Vector[T, 4],
    [T < Scalar].(T, Vector[T, 2], T) => Vector[T, 4],
    [T < Scalar].(T, T, Vector[T, 2]) => Vector[T, 4],
    [T < Scalar].(Vector[T, 2], T, T) => Vector[T, 4],
    [T < Scalar].(Vector[T, 2], Vector[T, 2]) => Vector[T, 4],
    [T < Scalar].(Vector[T, 3], T) => Vector[T, 4],
    [T < Scalar].(T, Vector[T, 3]) => Vector[T, 4],
}

(2..4).each do |columns|
    (2..4).each do |rows|
        operator :"mat#{columns}x#{rows}", {
            # FIXME: overload resolution should support explicitly instantiating variables
            # [T < ConcreteFloat, S < Float].(Matrix[S, columns, rows]) => Matrix[T, columns, rows],
            [T < Float].(Matrix[T, columns, rows]) => Matrix[T, columns, rows],
            [T < Float].(*([T] * columns * rows)) => Matrix[T, columns, rows],
            [T < Float].(*([Vector[T, rows]] * columns)) => Matrix[T, columns, rows],
        }
    end
end
