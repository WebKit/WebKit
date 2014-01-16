/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef SearchInputType_h
#define SearchInputType_h

#include "BaseTextInputType.h"
#include "Timer.h"

namespace WebCore {

class SearchFieldCancelButtonElement;
class SearchFieldResultsButtonElement;

class SearchInputType : public BaseTextInputType {
public:
    explicit SearchInputType(HTMLInputElement&);

    void stopSearchEventTimer();

private:
    virtual void attach() override;
    virtual void addSearchResult() override;
    virtual RenderPtr<RenderElement> createInputRenderer(PassRef<RenderStyle>) override;
    virtual const AtomicString& formControlType() const override;
    virtual bool shouldRespectSpeechAttribute() override;
    virtual bool isSearchField() const override;
    virtual bool needsContainer() const override;
    virtual void createShadowSubtree() override;
    virtual void destroyShadowSubtree() override;
    virtual HTMLElement* resultsButtonElement() const override;
    virtual HTMLElement* cancelButtonElement() const override;
    virtual void handleKeydownEvent(KeyboardEvent*) override;
    virtual void didSetValueByUserEdit(ValueChangeState) override;
    virtual bool sizeShouldIncludeDecoration(int defaultSize, int& preferredSize) const override;
    virtual float decorationWidth() const override;

    void searchEventTimerFired(Timer<SearchInputType>*);
    bool searchEventsShouldBeDispatched() const;
    void startSearchEventTimer();

    HTMLElement* m_resultsButton;
    HTMLElement* m_cancelButton;
    Timer<SearchInputType> m_searchEventTimer;
};

} // namespace WebCore

#endif // SearchInputType_h
