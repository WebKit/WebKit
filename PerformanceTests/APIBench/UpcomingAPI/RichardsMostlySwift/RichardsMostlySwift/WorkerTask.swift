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

import JavaScriptCore

class WorkerTask : Task {
    private var js : JSValue

    private static let context : JSContext = {
        let context = JSContext()!
        let source = try! String(contentsOfFile: "richards.js")
        context.evaluateScript(source)
        context.exceptionHandler = { context, exception in
            print(exception!.toString()!)
            exit(2)
        }
        return context
    }()
    private static let constructor : JSValue = {
        return context.objectForKeyedSubscript("createWorkerTask").call(withArguments: [Device.A.rawValue, Device.B.rawValue, WorkPacket.SIZE])
    }()

    static func create(scheduler: Scheduler, priority: Priority, queue: Packet)
    {
        let task = WorkerTask(scheduler: scheduler, priority: priority, queue: queue)
        scheduler.add(task: task)
    }

    override func run(packet: Packet?) -> Task?
    {
        return self.js.invokeMethod("run", withArguments: [packet]).toObject() as! Task?
    }

    private init(scheduler: Scheduler, priority: Priority, queue: Packet)
    {
        self.js = WorkerTask.constructor.construct(withArguments: [scheduler])
        super.init(scheduler: scheduler, priority: priority, queue: queue)
    }
}
