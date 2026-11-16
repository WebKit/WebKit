@_cdecl("trigger_fatal_error")
public func triggerFatalError(_ condition: Int32) -> Int32 {
    guard condition != 0 else {
        fatalError("condition must not be zero")
    }
    return condition * 2
}

@main
struct FatalErrorTest {
    static func main() {
        _ = triggerFatalError
    }
}
