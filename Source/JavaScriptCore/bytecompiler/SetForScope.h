#ifndef SetForScope_h
#define SetForScope_h

#include <wtf/StdLibExtras.h>

namespace JSC {

template<typename T>
class SetForScope {
public:
    SetForScope(T& currentValue, const T& newValue)
        : m_ref(currentValue)
        , m_oldValue(currentValue)
    {
        m_ref = newValue;
    }

    SetForScope(T& currentValue, T&& newValue)
        : m_ref(currentValue)
        , m_oldValue(currentValue)
    {
        currentValue = WTF::move(newValue);
    }

    ~SetForScope()
    {
        m_ref = m_oldValue;
    }

private:
    T& m_ref;
    T m_oldValue;
};

}; // namespace JSC

#endif // SetForScope_h
