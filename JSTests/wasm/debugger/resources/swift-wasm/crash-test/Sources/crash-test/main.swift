@inline(never)
private func _overflowTrap(_ x: Int32) -> Int32 { return x + 1 }

@_cdecl("crash_on_zero")
public func crashOnZero(_ value: Int32) -> Int32 {
    if value == 0 {
        let _ = _overflowTrap(Int32.max)
    }
    return value * 2
}

@main
struct CrashTest {
    static func main() {
        _ = crashOnZero
    }
}
