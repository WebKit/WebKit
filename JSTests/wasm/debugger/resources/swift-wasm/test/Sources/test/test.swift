func doubleValue(_ x: Int32) -> Int32 {
    x * 2
}

func addNumbers(_ a: Int32, _ b: Int32) -> Int32 {
    a + b
}

func isEven(_ n: Int32) -> Bool {
    n % 2 == 0
}

@_cdecl("process_number")
public func processNumber(_ input: Int32) -> Int32 {
    let doubled = doubleValue(input)
    let sum = addNumbers(input, doubled)
    guard isEven(sum) else {
        return sum + 5
    }
    return sum + 10
}

@main
struct Demo {
    static func main() {
        _ = processNumber
    }
}
