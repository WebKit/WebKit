/*
 * Copyright (C) 2021 Sony Interactive Entertainment Inc.
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

#include "URLBar.h"

#include <KeyboardCodes.h>

using namespace toolkitten;

void URLBar::setClient(Client&& client)
{
    m_client = std::move(client);
}

void URLBar::updateSelf()
{
    TextBox::updateSelf();

    if (focusedWidget() == this && m_imeDialog && m_imeDialog->isFinished()) {
        setText(m_imeDialog->getImeText().c_str());
        m_imeDialog.reset();
        if (m_client.onDecideURL)
            m_client.onDecideURL(this, getText());
    }
}

bool URLBar::onMouseMove(toolkitten::IntPoint)
{
    setFocused();
    return true;
}

bool URLBar::onKeyDown(int32_t virtualKeyCode)
{
    switch (virtualKeyCode) {
    case VK_RETURN:
        setFocused();
        onSelect();
        return true;
    }
    return false;
}

void URLBar::onSelect()
{
    EditorState editorState;
    editorState.inputType = InputFieldType::Url;
    editorState.supportLanguage = InputFieldLanguage::English_Us;
    editorState.title = "Input URL";
    editorState.text = std::string(getText());
    editorState.position = globalPosition();
    editorState.isInLoginForm = false;
    m_imeDialog = ImeDialog::create(editorState);
}
