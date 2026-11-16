@_cdecl("func_b")
public func funcB(_ x: Int32) -> Int32 {
    let incremented = x + 1
    return incremented * 2
}

@main
struct FuncB {
    static func main() {
        _ = funcB
    }
}
