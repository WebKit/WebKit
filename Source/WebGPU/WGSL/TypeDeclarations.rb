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
