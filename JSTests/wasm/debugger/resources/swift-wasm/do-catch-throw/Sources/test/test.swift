enum TestError: Error {
    case testException
    case anotherException
}

func throwingFunction() throws -> Int32 {
    throw TestError.testException
}

// Test 1: do-catch with conditional direct throw (no function call)
@_cdecl("test_do_throw_catch")
public func testDoThrowCatch(_ num: Int32) -> Int32 {
    do {
        if num % 2 == 0 {
            throw TestError.testException
        }
        return 23
    } catch {
        return 42
    }
}

// Test 2: Nested do-catch with direct throw
@_cdecl("test_do_throw_catch_nested")
public func testDoThrowCatchNested(_ num: Int32) -> Int32 {
    var result: Int32 = 0
    do {
        // Outer try block
        do {
            // Inner try block
            if num % 2 == 0 {
                throw TestError.testException
            }
            result = 23
        } catch TestError.testException {
            // Inner catch - rethrow as different exception
            throw TestError.anotherException
        }
        return result
    } catch TestError.anotherException {
        // Outer catch
        return 42
    } catch {
        return 99
    }
}

// Test 3: do-catch with throwing function call
@_cdecl("test_do_throw_func_catch")
public func testDoThrowFuncCatch() -> Int32 {
    do {
        let result = try throwingFunction()
        return result
    } catch {
        return 42
    }
}

// Test 4: Nested do-catch with throwing function call
@_cdecl("test_do_throw_func_catch_nested")
public func testDoThrowFuncCatchNested() -> Int32 {
    var result: Int32 = 0
    do {
        // Outer try block
        do {
            // Inner try block - call throwing function
            result += try throwingFunction()
            result = 3
        } catch TestError.testException {
            // Inner catch - rethrow as different exception
            throw TestError.anotherException
        }
        return result
    } catch TestError.anotherException {
        // Outer catch
        return 42
    } catch {
        return 99
    }
}

@main
struct TryCatchTest {
    static func main() {
        _ = testDoThrowCatch
        _ = testDoThrowCatchNested
        _ = testDoThrowFuncCatch
        _ = testDoThrowFuncCatchNested
    }
}
