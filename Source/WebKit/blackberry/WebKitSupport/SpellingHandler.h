/*
 * Copyright (C) Research In Motion Limited 2013. All rights reserved.
 */

#ifndef SpellingHandler_h
#define SpellingHandler_h

#include "TextCheckerClient.h"
#include "Timer.h"
#include "VisiblePosition.h"
#include "VisibleSelection.h"

#include <wtf/RefPtr.h>

/**
 * SpellingHandler
 */

namespace WebCore {
class Range;
class SpellChecker;
class VisibleSelection;
class VisiblePosition;
}

namespace BlackBerry {
namespace WebKit {

class InputHandler;

class SpellingHandler {
public:
    SpellingHandler(InputHandler*);
    ~SpellingHandler();

    void spellCheckTextBlock(WebCore::VisibleSelection&, WebCore::TextCheckingProcessType);
    bool isSpellCheckActive() { return m_isSpellCheckActive; }
    void setSpellCheckActive(bool active) { m_isSpellCheckActive = active; }

private:
    void createSpellCheckRequest(PassRefPtr<WebCore::Range> rangeForSpellCheckingPtr, WebCore::TextCheckingProcessType);
    void parseBlockForSpellChecking(WebCore::Timer<SpellingHandler>*);
    PassRefPtr<WebCore::Range> getRangeForSpellCheckWithFineGranularity(WebCore::VisiblePosition startPosition, WebCore::VisiblePosition endPosition);

    InputHandler* m_inputHandler;

    WebCore::Timer<SpellingHandler> m_timer;
    WebCore::VisiblePosition m_startOfCurrentLine;
    WebCore::VisiblePosition m_endOfCurrentLine;
    WebCore::TextCheckingProcessType m_textCheckingProcessType;
    bool m_isSpellCheckActive;
};

} // WebKit
} // BlackBerry

#endif // SpellingHandler_h
