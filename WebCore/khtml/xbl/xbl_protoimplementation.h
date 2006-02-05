namespace DOM {
    class DOMString;
}

#include "PlatformString.h"

namespace XBL
{

class XBLPrototypeBinding;

class XBLPrototypeMember
{
public:
    XBLPrototypeMember(const DOM::DOMString& name);
    virtual ~XBLPrototypeMember() { delete m_next; }
        
    void appendData(const DOM::DOMString& data);
        
    void setNext(XBLPrototypeMember* next) { m_next = next; }
    XBLPrototypeMember* next() const { return m_next; }
        
protected:
    DOM::DOMString m_name;
    DOM::DOMString m_data;
    XBLPrototypeMember* m_next;
};
    
class XBLPrototypeImplementation
{
public:
    XBLPrototypeImplementation(const DOM::DOMString& name, XBLPrototypeBinding* binding);
    ~XBLPrototypeImplementation() { delete m_member; }
    
    void setMember(XBLPrototypeMember* m) { m_member = m; }
    
private:
    DOM::DOMString m_name;
    XBLPrototypeBinding* m_binding;
    XBLPrototypeMember* m_member;
    bool m_compiled;
};

class XBLPrototypeParameter
{
public:
    XBLPrototypeParameter(const DOM::DOMString& name) :m_name(name), m_next(0) {}
    ~XBLPrototypeParameter() { delete m_next; }
    
    XBLPrototypeParameter* next() const { return m_next; }
    void setNext(XBLPrototypeParameter* next) { m_next = next; }

private:
    DOM::DOMString m_name;
    XBLPrototypeParameter* m_next;
};

class XBLPrototypeMethod: public XBLPrototypeMember
{
public:
    XBLPrototypeMethod(const DOM::DOMString& name);
    virtual ~XBLPrototypeMethod() { delete m_parameter; }
    
    virtual bool isConstructor() const;
    virtual bool isDestructor() const;
    
    void addParameter(const DOM::DOMString& name);
    XBLPrototypeParameter* parameter() const { return m_parameter; }
    
private:
    XBLPrototypeParameter* m_parameter;
};

class XBLPrototypeConstructor: public XBLPrototypeMethod
{
public:
    XBLPrototypeConstructor();
    virtual bool isConstructor() const;
};

class XBLPrototypeDestructor: public XBLPrototypeMethod
{
public:
    XBLPrototypeDestructor();
    virtual bool isDestructor() const;
};

class XBLPrototypeField : public XBLPrototypeMember
{
public:
    XBLPrototypeField(const DOM::DOMString& name, bool readonly);
    
private:
    bool m_readonly;
};

class XBLPrototypeProperty : public XBLPrototypeMember
{
public:
    XBLPrototypeProperty(const DOM::DOMString& name, bool readonly, 
                         const DOM::DOMString& onget,
                         const DOM::DOMString& onset);
    
    void appendGetterText(const DOM::DOMString& text);
    void appendSetterText(const DOM::DOMString& text);

private:
    DOM::DOMString m_setter;
    bool m_readonly;
};


}
