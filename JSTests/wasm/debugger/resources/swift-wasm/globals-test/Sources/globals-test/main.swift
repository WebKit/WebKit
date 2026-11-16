nonisolated(unsafe) var globalCounter1: Int32 = 0
nonisolated(unsafe) var globalCounter2: Int32 = 0
nonisolated(unsafe) var globalCounter3: Int32 = 0
nonisolated(unsafe) var globalCounter4: Int32 = 0

func helper() {
    globalCounter2 = 2
    globalCounter3 = 3
}

@_cdecl("increment_globals")
public func incrementGlobals(_ x: Int32) -> Int32 {
    globalCounter1 = 1
    helper()
    globalCounter4 &+= x
    return globalCounter4
}

@main
struct GlobalsTest {
    static func main() {
        _ = incrementGlobals
    }
}
