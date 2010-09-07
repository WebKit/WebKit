/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#ifndef WebEditCommand_h
#define WebEditCommand_h

#include <WebCore/EditCommand.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>

namespace WebKit {

class WebEditCommand : public RefCounted<WebEditCommand> {
public:
    static PassRefPtr<WebEditCommand> create(PassRefPtr<WebCore::EditCommand>);

    WebCore::EditCommand* command() const { return m_command.get(); }
    uint64_t commandID() const { return m_commandID; }

private:
    WebEditCommand(PassRefPtr<WebCore::EditCommand> command, uint64_t commandID)
        : m_command(command)
        , m_commandID(commandID)
    {
    }

    RefPtr<WebCore::EditCommand> m_command;
    uint64_t m_commandID;
};

} // namespace WebKit

#endif // WebEditCommand_h
