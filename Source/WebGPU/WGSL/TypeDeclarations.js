// FIXME: add all the missing type declarations here
type('+', [Number(T)], [T, T], T)
type('+', [Number(T), N], [Vector(T, N), T], T)
type('+', [Number(T), N], [T, Vector(T, N)], Vector(T, N))
type('+', [Number(T), N], [Vector(T, N), Vector(T, N)], Vector(T, N))
type('+', [Float(T), C, R], [Matrix(T, C, R), Matrix(T, C, R)], Matrix(T, C, R))
