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

#include "config.h"
#include "PushSubscriptionIdentifier.h"

#include <wtf/text/StringBuilder.h>

namespace WebCore {

// Do not change this without thinking about backwards compatibility. Topics are persisted in both PushDatabase and APS.
String makePushTopic(const PushSubscriptionSetIdentifier& set, const String& scope)
{
    StringBuilder builder;
    builder.reserveCapacity(
        set.bundleIdentifier.length() +
        (!set.pushPartition.isEmpty() ? 6 + set.pushPartition.length() : 0) +
        (set.dataStoreIdentifier ? 40 : 0) +
        1 + scope.length());
    builder.append(set.bundleIdentifier);
    if (!set.pushPartition.isEmpty())
        builder.append(" part:"_s, set.pushPartition);
    if (set.dataStoreIdentifier)
        builder.append(" ds:"_s, set.dataStoreIdentifier->toString());
    builder.append(" "_s, scope);
    return builder.toString();
}

String PushSubscriptionSetIdentifier::debugDescription() const
{
    StringBuilder builder;
    builder.reserveCapacity(
        1 + bundleIdentifier.length() +
        (!pushPartition.isEmpty() ? 6 + pushPartition.length() : 0) +
        (dataStoreIdentifier ? 12 : 0) + 1);
    builder.append("["_s, bundleIdentifier);
    if (!pushPartition.isEmpty())
        builder.append(" part:"_s, pushPartition);
    if (dataStoreIdentifier)
        builder.append(" ds:"_s, dataStoreIdentifier->toString(), 0, 8);
    builder.append("]"_s);
    return builder.toString();
}

} // namespace WebCore
