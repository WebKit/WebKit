#include "npapi.h"
#include "bindings/npfunctions.h"

// Non-standard event types can be passed to HandleEvent.
// npapi.h that comes with WebKit.framework adds these events.
#define getFocusEvent       (osEvt + 16)
#define loseFocusEvent      (osEvt + 17)
#define adjustCursorEvent   (osEvt + 18)
