/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#ifndef DictationCommandIOS_h
#define DictationCommandIOS_h

#include "CompositeEditCommand.h"

#import <wtf/RetainPtr.h>

typedef struct objc_object *id;

namespace WebCore {

class DictationCommandIOS : public CompositeEditCommand {
public:
    static PassRefPtr<DictationCommandIOS> create(Document& document, PassOwnPtr<Vector<Vector<String> > > dictationPhrase, RetainPtr<id> metadata)
    {
        return adoptRef(new DictationCommandIOS(document, dictationPhrase, metadata));
    }
    
    virtual ~DictationCommandIOS();
private:
    DictationCommandIOS(Document& document, PassOwnPtr<Vector<Vector<String> > > dictationPhrase, RetainPtr<id> metadata);
    
    virtual void doApply();
    virtual EditAction editingAction() const { return EditActionDictation; }
    
    OwnPtr<Vector<Vector<String> > > m_dictationPhrases;
    RetainPtr<id> m_metadata;
};

} // namespace WebCore

#endif
