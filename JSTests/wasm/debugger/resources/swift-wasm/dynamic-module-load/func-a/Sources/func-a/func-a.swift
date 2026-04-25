@_cdecl("func_a")
public func funcA(_ x: Int32) -> Int32 {
    let doubled = x * 2
    return doubled + 1
}

@main
struct FuncA {
    static func main() {
        _ = funcA
    }
}
