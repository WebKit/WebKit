/*
 * This file is part of the DOM implementation for WebCore.
 *
 * Copyright (C) 2006 Apple Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef DocumentMarker_h
#define DocumentMarker_h

#include <wtf/Forward.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/text/WTFString.h>

#if PLATFORM(IOS)
#import <wtf/RetainPtr.h>
typedef struct objc_object *id;
#endif

namespace WebCore {

class DocumentMarkerDetails;

// A range of a node within a document that is "marked", such as the range of a misspelled word.
// It optionally includes a description that could be displayed in the user interface.
// It also optionally includes a flag specifying whether the match is active, which is ignored
// for all types other than type TextMatch.
class DocumentMarker {
public:
    enum MarkerType {
        Spelling = 1 << 0,
        Grammar = 1 << 1,
        TextMatch = 1 << 2,
        // Text has been modified by spell correction, reversion of spell correction or other type of substitution. 
        // On some platforms, this prevents the text from being autocorrected again. On post Snow Leopard Mac OS X, 
        // if a Replacement marker contains non-empty description, a reversion UI will be shown.
        Replacement = 1 << 3,
        // Renderer needs to add underline indicating that the text has been modified by spell
        // correction. Text with Replacement marker doesn't necessarily has CorrectionIndicator
        // marker. For instance, after some text has been corrected, it will have both Replacement
        // and CorrectionIndicator. However, if user further modifies such text, we would remove
        // CorrectionIndicator marker, but retain Replacement marker.
        CorrectionIndicator = 1 << 4,
        // Correction suggestion has been offered, but got rejected by user.
        RejectedCorrection = 1 << 5,
        // Text has been modified by autocorrection. The description of this marker is the original text before autocorrection.
        Autocorrected = 1 << 6,
        // On some platforms, this prevents the text from being spellchecked again.
        SpellCheckingExemption = 1 << 7,
        // This marker indicates user has deleted an autocorrection starting at the end of the
        // range that bears this marker. In some platforms, if the user later inserts the same original
        // word again at this position, it will not be autocorrected again. The description of this
        // marker is the original word before autocorrection was applied.
        DeletedAutocorrection = 1 << 8,
        // This marker indicates that the range of text spanned by the marker is entered by voice dictation,
        // and it has alternative text.
        DictationAlternatives = 1 << 9,
#if ENABLE(TELEPHONE_NUMBER_DETECTION)
        TelephoneNumber = 1 << 10
#endif
#if PLATFORM(IOS)
        // FIXME: iOS should share the same Dictation marks as everyone else.
        DictationPhraseWithAlternatives = 1 << 11,
        DictationResult = 1 << 12,
#endif
    };

    class MarkerTypes {
    public:
        // The constructor is intentionally implicit to allow conversion from the bit-wise sum of above types
        MarkerTypes(unsigned mask) : m_mask(mask) { }

        bool contains(MarkerType type) const { return m_mask & type; }
        bool intersects(const MarkerTypes& types) const { return (m_mask & types.m_mask); }
        bool operator==(const MarkerTypes& other) const { return m_mask == other.m_mask; }

        void add(const MarkerTypes& types) { m_mask |= types.m_mask; }
        void remove(const MarkerTypes& types) { m_mask &= ~types.m_mask; }

    private:
        unsigned m_mask;
    };

    class AllMarkers : public MarkerTypes {
    public:
        AllMarkers()
#if !PLATFORM(IOS)
#if !ENABLE(TELEPHONE_NUMBER_DETECTION)
            : MarkerTypes(Spelling | Grammar | TextMatch | Replacement | CorrectionIndicator | RejectedCorrection | Autocorrected | SpellCheckingExemption | DeletedAutocorrection | DictationAlternatives)
#else
            : MarkerTypes(Spelling | Grammar | TextMatch | Replacement | CorrectionIndicator | RejectedCorrection | Autocorrected | SpellCheckingExemption | DeletedAutocorrection | DictationAlternatives | TelephoneNumber)
#endif // !ENABLE(TELEPHONE_NUMBER_DETECTION)
#else
            : MarkerTypes(Spelling | Grammar | TextMatch | Replacement | CorrectionIndicator | RejectedCorrection | Autocorrected | SpellCheckingExemption | DeletedAutocorrection | DictationAlternatives | TelephoneNumber | DictationPhraseWithAlternatives | DictationResult)
#endif // !PLATFORM(IOS)
        {
        }
    };

    DocumentMarker();
    DocumentMarker(MarkerType, unsigned startOffset, unsigned endOffset);
    DocumentMarker(MarkerType, unsigned startOffset, unsigned endOffset, const String& description);
#if PLATFORM(IOS)
    DocumentMarker(MarkerType, unsigned startOffset, unsigned endOffset, const String& description, const Vector<String>& alternatives, RetainPtr<id> metadata);
#endif
    DocumentMarker(unsigned startOffset, unsigned endOffset, bool activeMatch);
    DocumentMarker(MarkerType, unsigned startOffset, unsigned endOffset, PassRefPtr<DocumentMarkerDetails>);

    MarkerType type() const { return m_type; }
    unsigned startOffset() const { return m_startOffset; }
    unsigned endOffset() const { return m_endOffset; }

    const String& description() const;
    bool activeMatch() const;
    DocumentMarkerDetails* details() const;

    void setActiveMatch(bool);
    void clearDetails() { m_details.clear(); }

    // Offset modifications are done by DocumentMarkerController.
    // Other classes should not call following setters.
    void setStartOffset(unsigned offset) { m_startOffset = offset; }
    void setEndOffset(unsigned offset) { m_endOffset = offset; }
    void shiftOffsets(int delta);

#if PLATFORM(IOS)
    const Vector<String>& alternatives() const;
    void setAlternative(const String&, size_t index);
    id metadata() const;
    void setMetadata(id);
#endif

    bool operator==(const DocumentMarker& o) const
    {
        return type() == o.type() && startOffset() == o.startOffset() && endOffset() == o.endOffset();
    }

    bool operator!=(const DocumentMarker& o) const
    {
        return !(*this == o);
    }

private:
    MarkerType m_type;
    unsigned m_startOffset;
    unsigned m_endOffset;
#if PLATFORM(IOS)
    // FIXME: See <rdar://problem/9431249>.
    Vector<String> m_alternatives;
    RetainPtr<id> m_metadata;
#endif
    RefPtr<DocumentMarkerDetails> m_details;
};

inline DocumentMarkerDetails* DocumentMarker::details() const
{
    return m_details.get();
}

#if PLATFORM(IOS)
inline DocumentMarker::DocumentMarker(MarkerType type, unsigned startOffset, unsigned endOffset, const String&, const Vector<String>& alternatives, RetainPtr<id> metadata)
    : m_type(type)
    , m_startOffset(startOffset)
    , m_endOffset(endOffset)
    , m_alternatives(alternatives)
    , m_metadata(metadata)
{
    // FIXME: <rdar://problem/11306422> iOS should investigate cleaner merge with ToT Dictation support
    ASSERT(type == DictationPhraseWithAlternatives || type == DictationResult);
}
#endif

#if PLATFORM(IOS)
inline const Vector<String>& DocumentMarker::alternatives() const
{
    ASSERT(m_type == DocumentMarker::DictationPhraseWithAlternatives);
    return m_alternatives;
}

inline void DocumentMarker::setAlternative(const String& alternative, size_t index)
{
    ASSERT(m_type == DocumentMarker::DictationPhraseWithAlternatives);
    m_alternatives[index] = alternative;
}

inline id DocumentMarker::metadata() const
{
    return m_metadata.get();
}

inline void DocumentMarker::setMetadata(id metadata)
{
    m_metadata = metadata;
}
#endif

class DocumentMarkerDetails : public RefCounted<DocumentMarkerDetails>
{
public:
    DocumentMarkerDetails() { }
    virtual ~DocumentMarkerDetails();
    virtual bool isDescription() const { return false; }
    virtual bool isTextMatch() const { return false; }
};

} // namespace WebCore

#endif // DocumentMarker_h
