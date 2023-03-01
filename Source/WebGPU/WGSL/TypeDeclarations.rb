# FIXME: add all the missing type declarations here
operator :+, {
    [T < Number].(T, T) => T,
    [T < Number, N].(Vector[T, N], T) => Vector[T, N],
    [T < Number, N].(T, Vector[T, N]) => Vector[T, N],
    [T < Number, N].(Vector[T, N], Vector[T, N]) => Vector[T, N],
    [T < Float, C, R].(Matrix[T, C, R], Matrix[T, C, R]) => Matrix[T, C, R],
}
