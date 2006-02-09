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
class XBLTokenHandler: public khtml::XMLHandler
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
    XBLTokenHandler(DOM::DocumentImpl *_doc);
    ~XBLTokenHandler();
    
    XBLDocumentImpl* xblDocument() const;
    
    // Overrides from XMLTokenizer
    bool startElement(const QString& namespaceURI, const QString& localName, const QString& qName, 
                      const QXmlAttributes& attrs);
    bool endElement(const QString& namespaceURI, const QString& localName, const QString& qName);
    bool characters(const QString& ch);

protected:
    // Helper methods.
    void createBinding();
    void createHandler(const QXmlAttributes& attrs);
    void createResource(const QString& localName, const QXmlAttributes& attrs);
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
