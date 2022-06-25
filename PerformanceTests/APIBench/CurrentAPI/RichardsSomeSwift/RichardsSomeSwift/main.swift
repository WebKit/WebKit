/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
*/

import Foundation
import JavaScriptCore
import QuartzCore

let ID_HANDLER_A = 2
let ID_HANDLER_B = 3
let DATA_SIZE = 4

@objc protocol WorkerTaskExports : JSExport {
    var scheduler : JSValue { get }
    var v1 : Int { get }
    var v2 : Int { get }

    init(scheduler: JSValue, v1: Int, v2: Int)

    func run(_: JSValue)-> NSObject
    func toString() -> String
}

@objc public class WorkerTask : NSObject, WorkerTaskExports {
    dynamic var scheduler : JSValue
    dynamic var v1 : Int
    dynamic var v2 : Int

    required init(scheduler: JSValue, v1: Int, v2: Int)
    {
        self.scheduler = scheduler
        self.v1 = v1
        self.v2 = v2
    }

    func toString() -> String
    {
        return "WorkerTask"
    }

    func run(_ packet: JSValue) -> NSObject
    {
        if packet.isNull || packet.isUndefined {
            return self.scheduler.invokeMethod("suspendCurrent", withArguments: [])
        }

        self.v1 = ID_HANDLER_A + ID_HANDLER_B - self.v1
        packet.setValue(self.v1, forProperty: "id")
        packet.setValue(0, forProperty: "a1")
        for i in 0...DATA_SIZE {
            self.v2 += 1
            if self.v2 > 26 {
                self.v2 = 1
            }
            packet.forProperty("a2").setValue(self.v2, forProperty: i)
        }
        return self.scheduler.invokeMethod("queue", withArguments: [packet])
    }
}

guard let context = JSContext() else {
    exit(1)
}

context.setObject(WorkerTask.self, forKeyedSubscript: "WorkerTask" as NSString)
context.exceptionHandler = { context, exception in
    print(exception?.toString() as Any)
    exit(1)
}

let source = try String(contentsOfFile: "richards.js")
let result = context.evaluateScript(source)

let iterations = 50
let start = CACurrentMediaTime()
for _ in 1...iterations {
    context.objectForKeyedSubscript("runRichards").call(withArguments: [])
}
let end = CACurrentMediaTime()
print(Int((end-start) * 1000))
