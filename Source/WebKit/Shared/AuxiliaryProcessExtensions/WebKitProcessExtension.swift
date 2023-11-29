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
open class Grant: NSObject {
    let inner : _Capabilities.Grant

    init(inner: _Capabilities.Grant) {
        self.inner = inner
    }

    @objc(invalidateWithError:)
    open func invalidate() throws {
        try self.inner.invalidate()
    }
}

@_spi(Private)
@objc(WKNetworkingProcessExtension)
open class WKNetworkingProcessExtension : NSObject {
    @objc(sharedInstance)
    static public var sharedInstance: WKNetworkingProcessExtension? = nil
    required public override init() {
        super.init()
        WKNetworkingProcessExtension.sharedInstance = self
    }
}

extension WKNetworkingProcessExtension: NetworkingServiceExtension {
    @objc(handle:)
    open func handle(xpcConnection: xpc_connection_t) {
    }

    @objc(grant:name:error:)
    open func grant(domain: String, name: String) throws -> Any {
        let grant = try self._request(capabilities: _Capabilities.assertion(domain, name))
        return Grant(inner: grant)
    }
}
