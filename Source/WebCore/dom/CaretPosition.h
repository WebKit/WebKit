/*
 * Copyright (C) 2024 Igalia S.L. All rights reserved.
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

#include "ScriptWrappable.h"

namespace WebCore {

class DOMRect;
class Node;

class CaretPosition : public ScriptWrappable, public RefCounted<CaretPosition> {
    WTF_MAKE_TZONE_OR_ISO_ALLOCATED_EXPORT(CaretPosition, WEBCORE_EXPORT);
public:
    static Ref<CaretPosition> create(RefPtr<Node>&& offsetNode, unsigned offset) { return adoptRef(*new CaretPosition(WTFMove(offsetNode), offset)); }

    RefPtr<Node> offsetNode() const { return m_offsetNode; }
    unsigned offset() const { return m_offset; }

    RefPtr<DOMRect> getClientRect();

private:
    CaretPosition(RefPtr<Node>&& offsetNode, unsigned offset);

    RefPtr<Node> m_offsetNode;
    unsigned m_offset;
};

} // namespace WebCore
