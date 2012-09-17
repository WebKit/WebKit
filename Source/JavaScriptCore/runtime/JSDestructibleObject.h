#ifndef JSDestructibleObject_h
#define JSDestructibleObject_h

#include "JSObject.h"

namespace JSC {

struct ClassInfo;

class JSDestructibleObject : public JSNonFinalObject {
public:
    typedef JSNonFinalObject Base;

    const ClassInfo* classInfo() const { return m_classInfo; }

protected:
    JSDestructibleObject(JSGlobalData& globalData, Structure* structure, Butterfly* butterfly = 0)
        : JSNonFinalObject(globalData, structure, butterfly)
        , m_classInfo(structure->classInfo())
    {
        ASSERT(m_classInfo);
    }

private:
    const ClassInfo* m_classInfo;
};

inline const ClassInfo* JSCell::classInfo() const
{
    if (MarkedBlock::blockFor(this)->destructorType() == MarkedBlock::Normal)
        return static_cast<const JSDestructibleObject*>(this)->classInfo();
#if ENABLE(GC_VALIDATION)
    return m_structure.unvalidatedGet()->classInfo();
#else
    return m_structure->classInfo();
#endif
}

} // namespace JSC

#endif
