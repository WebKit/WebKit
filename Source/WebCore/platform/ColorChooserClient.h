#ifndef ColorChooserClient_h
#define ColorChooserClient_h

#if ENABLE(INPUT_TYPE_COLOR)

#include "ColorChooser.h"
#include "IntRect.h"
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/Vector.h>

namespace WebCore {

class Color;

class ColorChooserClient {
public:
    virtual ~ColorChooserClient() { }

    virtual void didChooseColor(const Color&) = 0;
    virtual void didEndChooser() = 0;
    virtual IntRect elementRectRelativeToRootView() const = 0;
    virtual Color currentColor() = 0;
    virtual bool shouldShowSuggestions() const = 0;
    virtual Vector<Color> suggestions() const = 0;
};

} // namespace WebCore

#endif // ENABLE(INPUT_TYPE_COLOR)

#endif // ColorChooserClient_h
