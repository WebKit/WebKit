namespace WebCore {
    class String;
}

#include "PlatformString.h"

namespace XBL
{

class XBLPrototypeBinding;

class XBLPrototypeMember
{
public:
    XBLPrototypeMember(const WebCore::String& name);
    virtual ~XBLPrototypeMember() { delete m_next; }
        
    void appendData(const WebCore::String& data);
        
    void setNext(XBLPrototypeMember* next) { m_next = next; }
    XBLPrototypeMember* next() const { return m_next; }
        
protected:
    WebCore::String m_name;
    WebCore::String m_data;
    XBLPrototypeMember* m_next;
};
    
class XBLPrototypeImplementation
{
public:
    XBLPrototypeImplementation(const WebCore::String& name, XBLPrototypeBinding* binding);
    ~XBLPrototypeImplementation() { delete m_member; }
    
    void setMember(XBLPrototypeMember* m) { m_member = m; }
    
private:
    WebCore::String m_name;
    XBLPrototypeBinding* m_binding;
    XBLPrototypeMember* m_member;
    bool m_compiled;
};

class XBLPrototypeParameter
{
public:
    XBLPrototypeParameter(const WebCore::String& name) :m_name(name), m_next(0) {}
    ~XBLPrototypeParameter() { delete m_next; }
    
    XBLPrototypeParameter* next() const { return m_next; }
    void setNext(XBLPrototypeParameter* next) { m_next = next; }

private:
    WebCore::String m_name;
    XBLPrototypeParameter* m_next;
};

class XBLPrototypeMethod: public XBLPrototypeMember
{
public:
    XBLPrototypeMethod(const WebCore::String& name);
    virtual ~XBLPrototypeMethod() { delete m_parameter; }
    
    virtual bool isConstructor() const;
    virtual bool isDestructor() const;
    
    void addParameter(const WebCore::String& name);
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
    XBLPrototypeField(const WebCore::String& name, bool readonly);
    
private:
    bool m_readonly;
};

class XBLPrototypeProperty : public XBLPrototypeMember
{
public:
    XBLPrototypeProperty(const WebCore::String& name, bool readonly, 
                         const WebCore::String& onget,
                         const WebCore::String& onset);
    
    void appendGetterText(const WebCore::String& text);
    void appendSetterText(const WebCore::String& text);

private:
    WebCore::String m_setter;
    bool m_readonly;
};


}
