/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#pragma once

#import <wtf/CompletionHandler.h>
#import <wtf/Function.h>
#import <wtf/Ref.h>
#import <wtf/RetainPtr.h>
#import <wtf/Vector.h>
#import <wtf/WeakHashSet.h>
#import <wtf/text/WTFString.h>

#if ENABLE(NETWORK_CONNECTION_INTEGRITY)
#import <WebCore/LookalikeCharactersSanitizationData.h>
#endif

OBJC_CLASS WKNetworkConnectionIntegrityNotificationListener;
OBJC_CLASS NSURLSession;

namespace WebKit {

#if ENABLE(NETWORK_CONNECTION_INTEGRITY)

void configureForNetworkConnectionIntegrity(NSURLSession *);
void requestAllowedLookalikeCharacterStrings(CompletionHandler<void(const Vector<WebCore::LookalikeCharactersSanitizationData>&)>&&);

class LookalikeCharacters {
public:
    static LookalikeCharacters& shared();

    const Vector<String>& cachedStrings() const { return m_cachedStrings; }
    void updateStrings(CompletionHandler<void()>&&);

    class Observer : public RefCounted<Observer>, public CanMakeWeakPtr<Observer> {
    public:
        ~Observer() = default;

    private:
        friend class LookalikeCharacters;

        Observer(Function<void()>&& callback)
            : m_callback { WTFMove(callback) }
        {
        }

        static Ref<Observer> create(Function<void()>&& callback)
        {
            return adoptRef(*new Observer(WTFMove(callback)));
        }

        void invokeCallback() { m_callback(); }

        Function<void()> m_callback;
    };

    Ref<Observer> observeUpdates(Function<void()>&&);

private:
    LookalikeCharacters() = default;
    void setCachedStrings(Vector<String>&&);

    RetainPtr<WKNetworkConnectionIntegrityNotificationListener> m_notificationListener;
    Vector<String> m_cachedStrings;
    WeakHashSet<Observer> m_observers;
};

void configureForNetworkConnectionIntegrity(NSURLSession *);

#endif // ENABLE(NETWORK_CONNECTION_INTEGRITY)

} // namespace WebKit
