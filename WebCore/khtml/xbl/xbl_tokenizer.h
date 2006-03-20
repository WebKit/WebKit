#include "xml_tokenizer.h"

namespace XBL {

class XBLPrototypeBinding;
class XBLPrototypeHandler;
class XBLPrototypeImplementation;
class XBLPrototypeMember;
class XBLPrototypeMethod;
class XBLPrototypeParameter;
class XBLPrototypeProperty;
class XBLPrototypeField;
class XBLPrototypeConstructor;
class XBLPrototypeDestructor;
class XBLDocument;

// This class is used to handle tokens returned from an XML parser for the purpose of building the
// custom data structures required by an XBL document.
class XBLTokenHandler: public WebCore::XMLHandler
{
typedef enum {
    eXBL_InDocument,
    eXBL_InBinding,
    eXBL_InResources,
    eXBL_InImplementation,
    eXBL_InHandlers,
    eXBL_InContent
} XBLPrimaryState;

typedef enum {
    eXBL_None,
    eXBL_InHandler,
    eXBL_InMethod,
    eXBL_InProperty,
    eXBL_InField,
    eXBL_InBody,
    eXBL_InGetter,
    eXBL_InSetter,
    eXBL_InConstructor,
    eXBL_InDestructor
} XBLSecondaryState;

public:
    XBLTokenHandler(WebCore::Document *_doc);
    ~XBLTokenHandler();
    
    XBLDocument* xblDocument() const;
    
    // Overrides from XMLTokenizer
    bool startElement(const DeprecatedString& namespaceURI, const DeprecatedString& localName, const DeprecatedString& qName, 
                      const QXmlAttributes& attrs);
    bool endElement(const DeprecatedString& namespaceURI, const DeprecatedString& localName, const DeprecatedString& qName);
    bool characters(const DeprecatedString& ch);

protected:
    // Helper methods.
    void createBinding();
    void createHandler(const QXmlAttributes& attrs);
    void createResource(const DeprecatedString& localName, const QXmlAttributes& attrs);
    void createImplementation(const QXmlAttributes& attrs);
    void createConstructor();
    void createDestructor();
    void createField(const QXmlAttributes& attrs);
    void createProperty(const QXmlAttributes& attrs);
    void createMethod(const QXmlAttributes& attrs);
    void createParameter(const QXmlAttributes& attrs);

    void addMember(XBLPrototypeMember* member);

private:
    XBLPrimaryState m_state;
    XBLSecondaryState m_secondaryState;
    
    XBLPrototypeBinding* m_binding;
    XBLPrototypeHandler* m_handler;
    XBLPrototypeImplementation* m_implementation;
    XBLPrototypeMember* m_member;
    XBLPrototypeProperty* m_property;
    XBLPrototypeMethod* m_method;
    XBLPrototypeField* m_field;
};

}
