/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

import ServiceExtensions
@_spi(Private) import ServiceExtensions

@objc
@_spi(Private)
open class Grant: WKGrant {
    let inner : _Capabilities.Grant

    init(inner: _Capabilities.Grant) {
        self.inner = inner
    }

    deinit {
        do {
            try invalidate()
        } catch {
            NSLog("Failed to invalidate grant")
        }
    }

    open func invalidate() throws {
        try self.inner.invalidate()
    }
}

@main
class NetworkingProcessExtension : WKProcessExtension {
    override required init() {
        super.init()
        setSharedInstance(self)
    }
}

extension NetworkingProcessExtension : NetworkingServiceExtension {
    func handle(xpcConnection: xpc_connection_t) {
        handleNewConnection(xpcConnection)
    }

    override func grant(_ domain: String, name: String) -> Any {
        do {
            let grant = try self._request(capabilities: _Capabilities.assertion(domain, name))
            return Grant(inner: grant)
        } catch {
            return WKGrant()
        }
    }

    override func lockdownSandbox(_ version: String) {
        if let lockdownVersion = _LockdownVersion(rawValue: version) {
            self._lockdown(version: lockdownVersion)
        }
    }

}
