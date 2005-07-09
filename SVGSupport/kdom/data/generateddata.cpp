#include <kdom/ecma/Ecma.h>
#include <kdom/ecma/EcmaInterface.h>
#include <kdom/ecma/DOMLookup.h>
#include <ProcessingInstruction.h>
#include <CSSRule.h>
#include <Attr.h>
#include <XPathEvaluator.h>
#include <LSParser.h>
#include <Text.h>
#include <LSException.h>
#include <CSSValueListImpl.h>
#include <LSResourceResolverImpl.h>
#include <Element.h>
#include <DocumentType.h>
#include <KDOMPart.h>
#include <DOMUserDataImpl.h>
#include <UIEventImpl.h>
#include <CSSImportRule.h>
#include <XPathNSResolver.h>
#include <CSSImportRuleImpl.h>
#include <DocumentFragment.h>
#include <KeyboardEvent.h>
#include <kdom/css/RGBColor.h>
#include <DocumentEventImpl.h>
#include <NamedNodeMap.h>
#include <ExprNodeImpl.h>
#include <CSSValueImpl.h>
#include <XPathNSResolverImpl.h>
#include <LSSerializerFilterImpl.h>
#include <AbstractView.h>
#include <StyleSheetListImpl.h>
#include <CDATASection.h>
#include <XPathEvaluatorImpl.h>
#include <DOMImplementationImpl.h>
#include <RenderStyle.h>
#include <CSSPrimitiveValue.h>
#include <OperatorImpl.h>
#include <LiteralImpl.h>
#include <LSOutput.h>
#include <KeyboardEventImpl.h>
#include <CharacterDataImpl.h>
#include <NodeIteratorImpl.h>
#include <CSSCharsetRuleImpl.h>
#include <CSSRuleListImpl.h>
#include <CDATASectionImpl.h>
#include <kdomxpath.h>
#include <EventTargetImpl.h>
#include <Event.h>
#include <NodeImpl.h>
#include <CSSUnknownRuleImpl.h>
#include <CSSStyleSheet.h>
#include <XPathExpression.h>
#include <DocumentTypeImpl.h>
#include <DOMLocatorImpl.h>
#include <ContextImpl.h>
#include <CSSPageRule.h>
#include <KDOMCacheHelper.h>
#include <KDOMCache.h>
#include <TreeWalker.h>
#include <KDOMCachedDocument.h>
#include <RGBColorImpl.h>
#include <DOMConfiguration.h>
#include <MutationEvent.h>
#include <EventImpl.h>
#include <DocumentEvent.h>
#include <XPathExpressionFilterImpl.h>
#include <KDOMCachedStyleSheet.h>
#include <EventExceptionImpl.h>
#include <KDOMCachedObjectClient.h>
#include <CSSFontFaceRule.h>
#include <DocumentImpl.h>
#include <NamedAttrMapImpl.h>
#include <LSInputImpl.h>
#include <XPathCustomExceptionImpl.h>
#include <RangeExceptionImpl.h>
#include <Node.h>
#include <LSOutputImpl.h>
#include <LSResourceResolver.h>
#include <Document.h>
#include <XPathFactory1Impl.h>
#include <DOMStringList.h>
#include <AxisImpl.h>
#include <MediaList.h>
#include <CSSRuleImpl.h>
#include <MouseEventImpl.h>
#include <NodeFilter.h>
#include <NodeFilterImpl.h>
#include <CSSStyleRule.h>
#include <CSSStyleSheetImpl.h>
#include <RangeImpl.h>
#include <Comment.h>
#include <StepImpl.h>
#include <DOMErrorHandler.h>
#include <DOMUserData.h>
#include <DOMException.h>
#include <DocumentTraversal.h>
#include <KDOMCachedImage.h>
#include <DOMError.h>
#include <EventListenerImpl.h>
#include <MutationEventImpl.h>
#include <Entity.h>
#include <CSSCharsetRule.h>
#include <KDOMView.h>
#include <XPathException.h>
#include <LinkStyle.h>
#include <DocumentFragmentImpl.h>
#include <CSSStyleDeclarationImpl.h>
#include <ViewCSS.h>
#include <TagNodeListImpl.h>
#include <CDFInterface.h>
#include <RangeException.h>
#include <ImageSource.h>
#include <DOMString.h>
#include <TypeInfoImpl.h>
#include <AttrImpl.h>
#include <DOMConfigurationImpl.h>
#include <CSSPrimitiveValueImpl.h>
#include <DOMStringImpl.h>
#include <kdom/Shared.h>
#include <kdom/css/impl/Font.h>
#include <XPathResultImpl.h>
#include <Range.h>
#include <DocumentCSS.h>
#include <TraversalImpl.h>
#include <CharacterData.h>
#include <VariableRefImpl.h>
#include <CSSStyleRuleImpl.h>
#include <DOMStringListImpl.h>
#include <Rect.h>
#include <CSSStyleDeclaration.h>
#include <NodeListImpl.h>
#include <StyleSheetImpl.h>
#include <LSExceptionImpl.h>
#include <NotationImpl.h>
#include <RenderStyleDefs.h>
#include <DocumentView.h>
#include <RectImpl.h>
#include <ProcessingInstructionImpl.h>
#include <LSSerializerFilter.h>
#include <RegisteredEventListener.h>
#include <LSParserFilterImpl.h>
#include <KDOMCachedScript.h>
#include <CSSPageRuleImpl.h>
#include <CSSFontFaceRuleImpl.h>
#include <DOMLocator.h>
#include <AbstractViewImpl.h>
#include <XPathResult.h>
#include <LSParserImpl.h>
#include <CSSValueList.h>
#include <Counter.h>
#include <XPathNamespace.h>
#include <XPathExceptionImpl.h>
#include <CounterImpl.h>
#include <XPathFactoryBaseImpl.h>
#include <XPathNamespaceImpl.h>
#include <UIEvent.h>
#include <KDOMCSSParser.h>
#include <DOMErrorHandlerImpl.h>
#include <CSSValue.h>
#include <LSParserFilter.h>
#include <DOMImplementation.h>
#include <DocumentViewImpl.h>
#include <Notation.h>
#include <StyleBaseImpl.h>
#include <DOMImplementationLS.h>
#include <StyleSheet.h>
#include <DOMErrorImpl.h>
#include <MouseEvent.h>
#include <TreeWalkerImpl.h>
#include <CSSMediaRuleImpl.h>
#include <NodeList.h>
#include <ElementImpl.h>
#include <EntityReferenceImpl.h>
#include <EventException.h>
#include <DOMExceptionImpl.h>
#include <TextImpl.h>
#include <CommentImpl.h>
#include <DocumentStyleImpl.h>
#include <CSSUnknownRule.h>
#include <LSSerializerImpl.h>
#include <EntityImpl.h>
#include <EventListener.h>
#include <LSSerializer.h>
#include <NamedNodeMapImpl.h>
#include <MediaListImpl.h>
#include <TypeInfo.h>
#include <StyleSheetList.h>
#include <kdom/css/impl/CSSStyleSelector.h>
#include <DocumentRange.h>
#include <DOMImplementationCSS.h>
#include <DocumentStyle.h>
#include <EntityReference.h>
#include <XMLElementImpl.h>
#include <XPathExpressionImpl.h>
#include <EventTarget.h>
#include <KDOMCachedObject.h>
#include <KDOMLoader.h>
#include <CSSRuleList.h>
#include <DocumentTraversalImpl.h>
#include <DocumentRangeImpl.h>
#include <CSSMediaRule.h>
#include <LSInput.h>
#include <NodeIterator.h>
#include <DOMObject.h>
#include <ScopeImpl.h>

using namespace KJS;
using namespace KDOM;

// For all classes with generated data: the ClassInfo
const ClassInfo AbstractView::s_classInfo = {"KDOM::AbstractView",0,&AbstractView::s_hashTable,0};
const ClassInfo Attr::s_classInfo = {"KDOM::Attr",0,&Attr::s_hashTable,0};
const ClassInfo CSSCharsetRule::s_classInfo = {"KDOM::CSSCharsetRule",0,&CSSCharsetRule::s_hashTable,0};
const ClassInfo CSSFontFaceRule::s_classInfo = {"KDOM::CSSFontFaceRule",0,&CSSFontFaceRule::s_hashTable,0};
const ClassInfo CSSImportRule::s_classInfo = {"KDOM::CSSImportRule",0,&CSSImportRule::s_hashTable,0};
const ClassInfo CSSMediaRule::s_classInfo = {"KDOM::CSSMediaRule",0,&CSSMediaRule::s_hashTable,0};
const ClassInfo CSSPageRule::s_classInfo = {"KDOM::CSSPageRule",0,&CSSPageRule::s_hashTable,0};
const ClassInfo CSSPrimitiveValue::s_classInfo = {"KDOM::CSSPrimitiveValue",0,&CSSPrimitiveValue::s_hashTable,0};
const ClassInfo CSSRule::s_classInfo = {"KDOM::CSSRule",0,&CSSRule::s_hashTable,0};
const ClassInfo CSSRuleList::s_classInfo = {"KDOM::CSSRuleList",0,&CSSRuleList::s_hashTable,0};
const ClassInfo CSSStyleDeclaration::s_classInfo = {"KDOM::CSSStyleDeclaration",0,&CSSStyleDeclaration::s_hashTable,0};
const ClassInfo CSSStyleRule::s_classInfo = {"KDOM::CSSStyleRule",0,&CSSStyleRule::s_hashTable,0};
const ClassInfo CSSStyleSheet::s_classInfo = {"KDOM::CSSStyleSheet",0,&CSSStyleSheet::s_hashTable,0};
const ClassInfo CSSValue::s_classInfo = {"KDOM::CSSValue",0,&CSSValue::s_hashTable,0};
const ClassInfo CSSValueList::s_classInfo = {"KDOM::CSSValueList",0,&CSSValueList::s_hashTable,0};
const ClassInfo CharacterData::s_classInfo = {"KDOM::CharacterData",0,&CharacterData::s_hashTable,0};
const ClassInfo Counter::s_classInfo = {"KDOM::Counter",0,&Counter::s_hashTable,0};
const ClassInfo DOMConfiguration::s_classInfo = {"KDOM::DOMConfiguration",0,&DOMConfiguration::s_hashTable,0};
const ClassInfo DOMError::s_classInfo = {"KDOM::DOMError",0,&DOMError::s_hashTable,0};
const ClassInfo DOMErrorHandler::s_classInfo = {"KDOM::DOMErrorHandler",0,&DOMErrorHandler::s_hashTable,0};
const ClassInfo DOMException::s_classInfo = {"KDOM::DOMException",0,&DOMException::s_hashTable,0};
const ClassInfo DOMImplementation::s_classInfo = {"KDOM::DOMImplementation",0,&DOMImplementation::s_hashTable,0};
const ClassInfo DOMLocator::s_classInfo = {"KDOM::DOMLocator",0,&DOMLocator::s_hashTable,0};
const ClassInfo DOMStringList::s_classInfo = {"KDOM::DOMStringList",0,&DOMStringList::s_hashTable,0};
const ClassInfo DOMUserData::s_classInfo = {"KDOM::DOMUserData",0,&DOMUserData::s_hashTable,0};
const ClassInfo Document::s_classInfo = {"KDOM::Document",0,&Document::s_hashTable,0};
const ClassInfo DocumentEvent::s_classInfo = {"KDOM::DocumentEvent",0,&DocumentEvent::s_hashTable,0};
const ClassInfo DocumentRange::s_classInfo = {"KDOM::DocumentRange",0,&DocumentRange::s_hashTable,0};
const ClassInfo DocumentStyle::s_classInfo = {"KDOM::DocumentStyle",0,&DocumentStyle::s_hashTable,0};
const ClassInfo DocumentTraversal::s_classInfo = {"KDOM::DocumentTraversal",0,&DocumentTraversal::s_hashTable,0};
const ClassInfo DocumentType::s_classInfo = {"KDOM::DocumentType",0,&DocumentType::s_hashTable,0};
const ClassInfo DocumentView::s_classInfo = {"KDOM::DocumentView",0,&DocumentView::s_hashTable,0};
const ClassInfo Element::s_classInfo = {"KDOM::Element",0,&Element::s_hashTable,0};
const ClassInfo Entity::s_classInfo = {"KDOM::Entity",0,&Entity::s_hashTable,0};
const ClassInfo Event::s_classInfo = {"KDOM::Event",0,&Event::s_hashTable,0};
const ClassInfo EventException::s_classInfo = {"KDOM::EventException",0,&EventException::s_hashTable,0};
const ClassInfo EventTarget::s_classInfo = {"KDOM::EventTarget",0,&EventTarget::s_hashTable,0};
const ClassInfo KeyboardEvent::s_classInfo = {"KDOM::KeyboardEvent",0,&KeyboardEvent::s_hashTable,0};
const ClassInfo LSException::s_classInfo = {"KDOM::LSException",0,&LSException::s_hashTable,0};
const ClassInfo LSInput::s_classInfo = {"KDOM::LSInput",0,&LSInput::s_hashTable,0};
const ClassInfo LSOutput::s_classInfo = {"KDOM::LSOutput",0,&LSOutput::s_hashTable,0};
const ClassInfo LSParser::s_classInfo = {"KDOM::LSParser",0,&LSParser::s_hashTable,0};
const ClassInfo LSParserFilter::s_classInfo = {"KDOM::LSParserFilter",0,&LSParserFilter::s_hashTable,0};
const ClassInfo LSResourceResolver::s_classInfo = {"KDOM::LSResourceResolver",0,&LSResourceResolver::s_hashTable,0};
const ClassInfo LSSerializer::s_classInfo = {"KDOM::LSSerializer",0,&LSSerializer::s_hashTable,0};
const ClassInfo LSSerializerFilter::s_classInfo = {"KDOM::LSSerializerFilter",0,&LSSerializerFilter::s_hashTable,0};
const ClassInfo MediaList::s_classInfo = {"KDOM::MediaList",0,&MediaList::s_hashTable,0};
const ClassInfo MouseEvent::s_classInfo = {"KDOM::MouseEvent",0,&MouseEvent::s_hashTable,0};
const ClassInfo MutationEvent::s_classInfo = {"KDOM::MutationEvent",0,&MutationEvent::s_hashTable,0};
const ClassInfo NamedNodeMap::s_classInfo = {"KDOM::NamedNodeMap",0,&NamedNodeMap::s_hashTable,0};
const ClassInfo Node::s_classInfo = {"KDOM::Node",0,&Node::s_hashTable,0};
const ClassInfo NodeIterator::s_classInfo = {"KDOM::NodeIterator",0,&NodeIterator::s_hashTable,0};
const ClassInfo NodeList::s_classInfo = {"KDOM::NodeList",0,&NodeList::s_hashTable,0};
const ClassInfo Notation::s_classInfo = {"KDOM::Notation",0,&Notation::s_hashTable,0};
const ClassInfo ProcessingInstruction::s_classInfo = {"KDOM::ProcessingInstruction",0,&ProcessingInstruction::s_hashTable,0};
const ClassInfo KDOM::RGBColor::s_classInfo = {"KDOM::RGBColor",0,&KDOM::RGBColor::s_hashTable,0};
const ClassInfo Range::s_classInfo = {"KDOM::Range",0,&Range::s_hashTable,0};
const ClassInfo RangeException::s_classInfo = {"KDOM::RangeException",0,&RangeException::s_hashTable,0};
const ClassInfo KDOM::Rect::s_classInfo = {"KDOM::Rect",0,&KDOM::Rect::s_hashTable,0};
const ClassInfo StyleSheet::s_classInfo = {"KDOM::StyleSheet",0,&StyleSheet::s_hashTable,0};
const ClassInfo StyleSheetList::s_classInfo = {"KDOM::StyleSheetList",0,&StyleSheetList::s_hashTable,0};
const ClassInfo Text::s_classInfo = {"KDOM::Text",0,&Text::s_hashTable,0};
const ClassInfo TreeWalker::s_classInfo = {"KDOM::TreeWalker",0,&TreeWalker::s_hashTable,0};
const ClassInfo TypeInfo::s_classInfo = {"KDOM::TypeInfo",0,&TypeInfo::s_hashTable,0};
const ClassInfo UIEvent::s_classInfo = {"KDOM::UIEvent",0,&UIEvent::s_hashTable,0};
const ClassInfo XPathEvaluator::s_classInfo = {"KDOM::XPathEvaluator",0,&XPathEvaluator::s_hashTable,0};
const ClassInfo XPathException::s_classInfo = {"KDOM::XPathException",0,&XPathException::s_hashTable,0};
const ClassInfo XPathExpression::s_classInfo = {"KDOM::XPathExpression",0,&XPathExpression::s_hashTable,0};
const ClassInfo XPathNSResolver::s_classInfo = {"KDOM::XPathNSResolver",0,&XPathNSResolver::s_hashTable,0};
const ClassInfo XPathNamespace::s_classInfo = {"KDOM::XPathNamespace",0,&XPathNamespace::s_hashTable,0};
const ClassInfo XPathResult::s_classInfo = {"KDOM::XPathResult",0,&XPathResult::s_hashTable,0};

bool AbstractView::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&AbstractView::s_hashTable,p2);
    if(e) return true;
    Q_UNUSED(p1);
    return false;
}

Value AbstractView::get(GET_METHOD_ARGS) const
{
    return lookupGetValue<AbstractView>(p1,p2,&s_hashTable,this,p3);
}

AbstractView KDOM::toAbstractView(KJS::ExecState *exec, const ObjectImp *p1)
{
	Q_UNUSED(exec);
    { const DOMBridge<AbstractView> *test = dynamic_cast<const DOMBridge<AbstractView> * >(p1);
    if(test) return AbstractView(test->impl()); }
    { const DOMBridge<ViewCSS> *test = dynamic_cast<const DOMBridge<ViewCSS> * >(p1);
    if(test) return AbstractView(test->impl()); }
    return AbstractView::null;
}

Value AbstractView::getInParents(GET_METHOD_ARGS) const
{
    Q_UNUSED(p1); Q_UNUSED(p2); Q_UNUSED(p3);
    return Undefined();
}

Object AbstractView::prototype(ExecState *p1) const
{
    if(p1) return p1->interpreter()->builtinObjectPrototype();
    return Object::dynamicCast(Null());
}

ObjectImp *AbstractView::bridge(ExecState *p1) const
{
    return new DOMBridge<AbstractView>(p1,static_cast<AbstractView::Private *>(d));
}

Value AbstractView::cache(ExecState *p1) const
{
    return cacheDOMObject<AbstractView>(p1,static_cast<AbstractView::Private *>(d));
}

bool Attr::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&Attr::s_hashTable,p2);
    if(e) return true;
    if(Node::hasProperty(p1,p2)) return true;
    return false;
}

Value Attr::get(GET_METHOD_ARGS) const
{
    return lookupGetValue<Attr>(p1,p2,&s_hashTable,this,p3);
}

Attr KDOM::toAttr(KJS::ExecState *exec, const ObjectImp *p1)
{
	Q_UNUSED(exec);
    { const DOMBridge<Attr> *test = dynamic_cast<const DOMBridge<Attr> * >(p1);
    if(test) return Attr(test->impl()); }
    return Attr::null;
}

Value Attr::getInParents(GET_METHOD_ARGS) const
{
    if(Node::hasProperty(p1,p2)) return Node::get(p1,p2,p3);
    return Undefined();
}

bool Attr::put(PUT_METHOD_ARGS)
{
    return lookupPut<Attr>(p1,p2,p3,p4,&s_hashTable,this);
}

bool Attr::putInParents(PUT_METHOD_ARGS)
{
    if(Node::hasProperty(p1,p2)) {
        Node::put(p1,p2,p3,p4);
        return true;
    }
    return false;
}

Object Attr::prototype(ExecState *p1) const
{
    if(p1) return p1->interpreter()->builtinObjectPrototype();
    return Object::dynamicCast(Null());
}

ObjectImp *Attr::bridge(ExecState *p1) const
{
    return new DOMRWBridge<Attr>(p1,static_cast<Attr::Private *>(d));
}

Value Attr::cache(ExecState *p1) const
{
    return cacheDOMObject<Attr>(p1,static_cast<Attr::Private *>(d));
}

bool CSSCharsetRule::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&CSSCharsetRule::s_hashTable,p2);
    if(e) return true;
    if(CSSRule::hasProperty(p1,p2)) return true;
    return false;
}

Value CSSCharsetRule::get(GET_METHOD_ARGS) const
{
    return lookupGetValue<CSSCharsetRule>(p1,p2,&s_hashTable,this,p3);
}

Value CSSCharsetRule::getInParents(GET_METHOD_ARGS) const
{
    if(CSSRule::hasProperty(p1,p2)) return CSSRule::get(p1,p2,p3);
    return Undefined();
}

bool CSSCharsetRule::put(PUT_METHOD_ARGS)
{
    return lookupPut<CSSCharsetRule>(p1,p2,p3,p4,&s_hashTable,this);
}

bool CSSCharsetRule::putInParents(PUT_METHOD_ARGS)
{
    if(CSSRule::hasProperty(p1,p2)) {
        CSSRule::put(p1,p2,p3,p4);
        return true;
    }
    return false;
}

Object CSSCharsetRule::prototype(ExecState *p1) const
{
    if(p1) return p1->interpreter()->builtinObjectPrototype();
    return Object::dynamicCast(Null());
}

ObjectImp *CSSCharsetRule::bridge(ExecState *p1) const
{
    return new DOMRWBridge<CSSCharsetRule>(p1,static_cast<CSSCharsetRule::Private *>(d));
}

Value CSSCharsetRule::cache(ExecState *p1) const
{
    return cacheDOMObject<CSSCharsetRule>(p1,static_cast<CSSCharsetRule::Private *>(d));
}

bool CSSFontFaceRule::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&CSSFontFaceRule::s_hashTable,p2);
    if(e) return true;
    if(CSSRule::hasProperty(p1,p2)) return true;
    return false;
}

Value CSSFontFaceRule::get(GET_METHOD_ARGS) const
{
    return lookupGetValue<CSSFontFaceRule>(p1,p2,&s_hashTable,this,p3);
}

Value CSSFontFaceRule::getInParents(GET_METHOD_ARGS) const
{
    if(CSSRule::hasProperty(p1,p2)) return CSSRule::get(p1,p2,p3);
    return Undefined();
}

bool CSSFontFaceRule::put(PUT_METHOD_ARGS)
{
    return lookupPut<CSSFontFaceRule>(p1,p2,p3,p4,&s_hashTable,this);
}

bool CSSFontFaceRule::putInParents(PUT_METHOD_ARGS)
{
    if(CSSRule::hasProperty(p1,p2)) {
        CSSRule::put(p1,p2,p3,p4);
        return true;
    }
    return false;
}

Object CSSFontFaceRule::prototype(ExecState *p1) const
{
    if(p1) return p1->interpreter()->builtinObjectPrototype();
    return Object::dynamicCast(Null());
}

ObjectImp *CSSFontFaceRule::bridge(ExecState *p1) const
{
    return new DOMRWBridge<CSSFontFaceRule>(p1,static_cast<CSSFontFaceRule::Private *>(d));
}

Value CSSFontFaceRule::cache(ExecState *p1) const
{
    return cacheDOMObject<CSSFontFaceRule>(p1,static_cast<CSSFontFaceRule::Private *>(d));
}

bool CSSImportRule::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&CSSImportRule::s_hashTable,p2);
    if(e) return true;
    if(CSSRule::hasProperty(p1,p2)) return true;
    return false;
}

Value CSSImportRule::get(GET_METHOD_ARGS) const
{
    return lookupGetValue<CSSImportRule>(p1,p2,&s_hashTable,this,p3);
}

Value CSSImportRule::getInParents(GET_METHOD_ARGS) const
{
    if(CSSRule::hasProperty(p1,p2)) return CSSRule::get(p1,p2,p3);
    return Undefined();
}

bool CSSImportRule::put(PUT_METHOD_ARGS)
{
    return lookupPut<CSSImportRule>(p1,p2,p3,p4,&s_hashTable,this);
}

bool CSSImportRule::putInParents(PUT_METHOD_ARGS)
{
    if(CSSRule::hasProperty(p1,p2)) {
        CSSRule::put(p1,p2,p3,p4);
        return true;
    }
    return false;
}

Object CSSImportRule::prototype(ExecState *p1) const
{
    if(p1) return p1->interpreter()->builtinObjectPrototype();
    return Object::dynamicCast(Null());
}

ObjectImp *CSSImportRule::bridge(ExecState *p1) const
{
    return new DOMRWBridge<CSSImportRule>(p1,static_cast<CSSImportRule::Private *>(d));
}

Value CSSImportRule::cache(ExecState *p1) const
{
    return cacheDOMObject<CSSImportRule>(p1,static_cast<CSSImportRule::Private *>(d));
}

bool CSSMediaRule::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&CSSMediaRule::s_hashTable,p2);
    if(e) return true;
    Object proto = CSSMediaRuleProto::self(p1);
    if(proto.hasProperty(p1,p2)) return true;
    if(CSSRule::hasProperty(p1,p2)) return true;
    return false;
}

Value CSSMediaRule::get(GET_METHOD_ARGS) const
{
    return lookupGet<CSSMediaRuleProtoFunc,CSSMediaRule>(p1,p2,&s_hashTable,this,p3);
}

CSSMediaRule CSSMediaRuleProtoFunc::cast(KJS::ExecState *exec, const ObjectImp *p1) const
{
    { const DOMBridge<CSSMediaRule> *test = dynamic_cast<const DOMBridge<CSSMediaRule> * >(p1);
    if(test) return CSSMediaRule(static_cast<CSSMediaRule::Private *>(test->impl())); }
    if(!exec) return CSSMediaRule::null;
    ScriptInterpreter *interpreter = static_cast<ScriptInterpreter *>(exec->interpreter());
    DocumentImpl *document = (interpreter ? interpreter->document() : 0);
    Ecma *engine = (document ? document->ecmaEngine() : 0);
    EcmaInterface *interface = (engine ? engine->interface() : 0);
    if(!interface) return CSSMediaRule::null;
    return interface->inheritedCSSMediaRuleCast(p1);
}

CSSMediaRule KDOM::EcmaInterface::inheritedCSSMediaRuleCast(const ObjectImp *p1)
{
	Q_UNUSED(p1); return CSSMediaRule::null;
}

Value CSSMediaRule::getInParents(GET_METHOD_ARGS) const
{
    Object proto = CSSMediaRuleProto::self(p1);
    if(proto.hasProperty(p1,p2)) return proto.get(p1,p2);
    if(CSSRule::hasProperty(p1,p2)) return CSSRule::get(p1,p2,p3);
    return Undefined();
}

bool CSSMediaRule::put(PUT_METHOD_ARGS)
{
    return lookupPut<CSSMediaRule>(p1,p2,p3,p4,&s_hashTable,this);
}

bool CSSMediaRule::putInParents(PUT_METHOD_ARGS)
{
    if(CSSRule::hasProperty(p1,p2)) {
        CSSRule::put(p1,p2,p3,p4);
        return true;
    }
    return false;
}

Object CSSMediaRule::prototype(ExecState *p1) const
{
    if(p1) return CSSMediaRuleProto::self(p1);
    return Object::dynamicCast(Null());
}

ObjectImp *CSSMediaRule::bridge(ExecState *p1) const
{
    return new DOMRWBridge<CSSMediaRule>(p1,static_cast<CSSMediaRule::Private *>(d));
}

Value CSSMediaRule::cache(ExecState *p1) const
{
    return cacheDOMObject<CSSMediaRule>(p1,static_cast<CSSMediaRule::Private *>(d));
}

bool CSSPageRule::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&CSSPageRule::s_hashTable,p2);
    if(e) return true;
    if(CSSRule::hasProperty(p1,p2)) return true;
    return false;
}

Value CSSPageRule::get(GET_METHOD_ARGS) const
{
    return lookupGetValue<CSSPageRule>(p1,p2,&s_hashTable,this,p3);
}

Value CSSPageRule::getInParents(GET_METHOD_ARGS) const
{
    if(CSSRule::hasProperty(p1,p2)) return CSSRule::get(p1,p2,p3);
    return Undefined();
}

bool CSSPageRule::put(PUT_METHOD_ARGS)
{
    return lookupPut<CSSPageRule>(p1,p2,p3,p4,&s_hashTable,this);
}

bool CSSPageRule::putInParents(PUT_METHOD_ARGS)
{
    if(CSSRule::hasProperty(p1,p2)) {
        CSSRule::put(p1,p2,p3,p4);
        return true;
    }
    return false;
}

Object CSSPageRule::prototype(ExecState *p1) const
{
    if(p1) return p1->interpreter()->builtinObjectPrototype();
    return Object::dynamicCast(Null());
}

ObjectImp *CSSPageRule::bridge(ExecState *p1) const
{
    return new DOMRWBridge<CSSPageRule>(p1,static_cast<CSSPageRule::Private *>(d));
}

Value CSSPageRule::cache(ExecState *p1) const
{
    return cacheDOMObject<CSSPageRule>(p1,static_cast<CSSPageRule::Private *>(d));
}

bool CSSPrimitiveValue::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&CSSPrimitiveValue::s_hashTable,p2);
    if(e) return true;
    Object proto = CSSPrimitiveValueProto::self(p1);
    if(proto.hasProperty(p1,p2)) return true;
    if(CSSValue::hasProperty(p1,p2)) return true;
    return false;
}

Value CSSPrimitiveValue::get(GET_METHOD_ARGS) const
{
    return lookupGet<CSSPrimitiveValueProtoFunc,CSSPrimitiveValue>(p1,p2,&s_hashTable,this,p3);
}

CSSPrimitiveValue CSSPrimitiveValueProtoFunc::cast(KJS::ExecState *exec, const ObjectImp *p1) const
{
    { const DOMBridge<CSSPrimitiveValue> *test = dynamic_cast<const DOMBridge<CSSPrimitiveValue> * >(p1);
    if(test) return CSSPrimitiveValue(static_cast<CSSPrimitiveValue::Private *>(test->impl())); }
    if(!exec) return CSSPrimitiveValue::null;
    ScriptInterpreter *interpreter = static_cast<ScriptInterpreter *>(exec->interpreter());
    DocumentImpl *document = (interpreter ? interpreter->document() : 0);
    Ecma *engine = (document ? document->ecmaEngine() : 0);
    EcmaInterface *interface = (engine ? engine->interface() : 0);
    if(!interface) return CSSPrimitiveValue::null;
    return interface->inheritedCSSPrimitiveValueCast(p1);
}

CSSPrimitiveValue KDOM::EcmaInterface::inheritedCSSPrimitiveValueCast(const ObjectImp *p1)
{
	Q_UNUSED(p1); return CSSPrimitiveValue::null;
}

Value CSSPrimitiveValue::getInParents(GET_METHOD_ARGS) const
{
    Object proto = CSSPrimitiveValueProto::self(p1);
    if(proto.hasProperty(p1,p2)) return proto.get(p1,p2);
    if(CSSValue::hasProperty(p1,p2)) return CSSValue::get(p1,p2,p3);
    return Undefined();
}

bool CSSPrimitiveValue::put(PUT_METHOD_ARGS)
{
    return lookupPut<CSSPrimitiveValue>(p1,p2,p3,p4,&s_hashTable,this);
}

bool CSSPrimitiveValue::putInParents(PUT_METHOD_ARGS)
{
    if(CSSValue::hasProperty(p1,p2)) {
        CSSValue::put(p1,p2,p3,p4);
        return true;
    }
    return false;
}

Object CSSPrimitiveValue::prototype(ExecState *p1) const
{
    if(p1) return CSSPrimitiveValueProto::self(p1);
    return Object::dynamicCast(Null());
}

ObjectImp *CSSPrimitiveValue::bridge(ExecState *p1) const
{
    return new DOMRWBridge<CSSPrimitiveValue>(p1,static_cast<CSSPrimitiveValue::Private *>(d));
}

Value CSSPrimitiveValue::cache(ExecState *p1) const
{
    return cacheDOMObject<CSSPrimitiveValue>(p1,static_cast<CSSPrimitiveValue::Private *>(d));
}

bool CSSRule::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&CSSRule::s_hashTable,p2);
    if(e) return true;
    Q_UNUSED(p1);
    return false;
}

Value CSSRule::get(GET_METHOD_ARGS) const
{
    return lookupGetValue<CSSRule>(p1,p2,&s_hashTable,this,p3);
}

Value CSSRule::getInParents(GET_METHOD_ARGS) const
{
    Q_UNUSED(p1); Q_UNUSED(p2); Q_UNUSED(p3);
    return Undefined();
}

bool CSSRule::put(PUT_METHOD_ARGS)
{
    return lookupPut<CSSRule>(p1,p2,p3,p4,&s_hashTable,this);
}

bool CSSRule::putInParents(PUT_METHOD_ARGS)
{
    Q_UNUSED(p1); Q_UNUSED(p2); Q_UNUSED(p3); Q_UNUSED(p4);
    return false;
}

Object CSSRule::prototype(ExecState *p1) const
{
    if(p1) return p1->interpreter()->builtinObjectPrototype();
    return Object::dynamicCast(Null());
}

ObjectImp *CSSRule::bridge(ExecState *p1) const
{
    return new DOMRWBridge<CSSRule>(p1,static_cast<CSSRule::Private *>(d));
}

Value CSSRule::cache(ExecState *p1) const
{
    return cacheDOMObject<CSSRule>(p1,static_cast<CSSRule::Private *>(d));
}

bool CSSRuleList::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&CSSRuleList::s_hashTable,p2);
    if(e) return true;
    Object proto = CSSRuleListProto::self(p1);
    if(proto.hasProperty(p1,p2)) return true;
    bool ok;
    unsigned int i = p2.toArrayIndex(&ok);
    if(ok && i < length()) return true;
    return false;
}

Value CSSRuleList::get(GET_METHOD_ARGS) const
{
    bool ok;
    unsigned int i = p2.toArrayIndex(&ok);
    if(ok && i < length()) return item(i).cache(p1);
    return lookupGet<CSSRuleListProtoFunc,CSSRuleList>(p1,p2,&s_hashTable,this,p3);
}

CSSRuleList CSSRuleListProtoFunc::cast(KJS::ExecState *exec, const ObjectImp *p1) const
{
    { const DOMBridge<CSSRuleList> *test = dynamic_cast<const DOMBridge<CSSRuleList> * >(p1);
    if(test) return CSSRuleList(static_cast<CSSRuleList::Private *>(test->impl())); }
    if(!exec) return CSSRuleList::null;
    ScriptInterpreter *interpreter = static_cast<ScriptInterpreter *>(exec->interpreter());
    DocumentImpl *document = (interpreter ? interpreter->document() : 0);
    Ecma *engine = (document ? document->ecmaEngine() : 0);
    EcmaInterface *interface = (engine ? engine->interface() : 0);
    if(!interface) return CSSRuleList::null;
    return interface->inheritedCSSRuleListCast(p1);
}

CSSRuleList KDOM::EcmaInterface::inheritedCSSRuleListCast(const ObjectImp *p1)
{
	Q_UNUSED(p1); return CSSRuleList::null;
}

Value CSSRuleList::getInParents(GET_METHOD_ARGS) const
{
    Object proto = CSSRuleListProto::self(p1);
    if(proto.hasProperty(p1,p2)) return proto.get(p1,p2);
    Q_UNUSED(p3);
    return Undefined();
}

Object CSSRuleList::prototype(ExecState *p1) const
{
    if(p1) return CSSRuleListProto::self(p1);
    return Object::dynamicCast(Null());
}

ObjectImp *CSSRuleList::bridge(ExecState *p1) const
{
    return new DOMBridge<CSSRuleList>(p1,static_cast<CSSRuleList::Private *>(d));
}

Value CSSRuleList::cache(ExecState *p1) const
{
    return cacheDOMObject<CSSRuleList>(p1,static_cast<CSSRuleList::Private *>(d));
}

bool CSSStyleDeclaration::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&CSSStyleDeclaration::s_hashTable,p2);
    if(e) return true;
    Object proto = CSSStyleDeclarationProto::self(p1);
    if(proto.hasProperty(p1,p2)) return true;
    bool ok;
    unsigned int i = p2.toArrayIndex(&ok);
    if(ok && i < length()) return true;
    return false;
}

Value CSSStyleDeclaration::get(GET_METHOD_ARGS) const
{
    bool ok;
    unsigned int i = p2.toArrayIndex(&ok);
    if(ok && i < length()) return getDOMString(item(i));
    return lookupGet<CSSStyleDeclarationProtoFunc,CSSStyleDeclaration>(p1,p2,&s_hashTable,this,p3);
}

CSSStyleDeclaration CSSStyleDeclarationProtoFunc::cast(KJS::ExecState *exec, const ObjectImp *p1) const
{
    { const DOMBridge<CSSStyleDeclaration> *test = dynamic_cast<const DOMBridge<CSSStyleDeclaration> * >(p1);
    if(test) return CSSStyleDeclaration(static_cast<CSSStyleDeclaration::Private *>(test->impl())); }
    if(!exec) return CSSStyleDeclaration::null;
    ScriptInterpreter *interpreter = static_cast<ScriptInterpreter *>(exec->interpreter());
    DocumentImpl *document = (interpreter ? interpreter->document() : 0);
    Ecma *engine = (document ? document->ecmaEngine() : 0);
    EcmaInterface *interface = (engine ? engine->interface() : 0);
    if(!interface) return CSSStyleDeclaration::null;
    return interface->inheritedCSSStyleDeclarationCast(p1);
}

CSSStyleDeclaration KDOM::EcmaInterface::inheritedCSSStyleDeclarationCast(const ObjectImp *p1)
{
	Q_UNUSED(p1); return CSSStyleDeclaration::null;
}

Value CSSStyleDeclaration::getInParents(GET_METHOD_ARGS) const
{
    Object proto = CSSStyleDeclarationProto::self(p1);
    if(proto.hasProperty(p1,p2)) return proto.get(p1,p2);
    Q_UNUSED(p3);
    return Undefined();
}

bool CSSStyleDeclaration::put(PUT_METHOD_ARGS)
{
    return lookupPut<CSSStyleDeclaration>(p1,p2,p3,p4,&s_hashTable,this);
}

bool CSSStyleDeclaration::putInParents(PUT_METHOD_ARGS)
{
    Q_UNUSED(p1); Q_UNUSED(p2); Q_UNUSED(p3); Q_UNUSED(p4);
    return false;
}

Object CSSStyleDeclaration::prototype(ExecState *p1) const
{
    if(p1) return CSSStyleDeclarationProto::self(p1);
    return Object::dynamicCast(Null());
}

ObjectImp *CSSStyleDeclaration::bridge(ExecState *p1) const
{
    return new DOMRWBridge<CSSStyleDeclaration>(p1,static_cast<CSSStyleDeclaration::Private *>(d));
}

Value CSSStyleDeclaration::cache(ExecState *p1) const
{
    return cacheDOMObject<CSSStyleDeclaration>(p1,static_cast<CSSStyleDeclaration::Private *>(d));
}

bool CSSStyleRule::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&CSSStyleRule::s_hashTable,p2);
    if(e) return true;
    if(CSSRule::hasProperty(p1,p2)) return true;
    return false;
}

Value CSSStyleRule::get(GET_METHOD_ARGS) const
{
    return lookupGetValue<CSSStyleRule>(p1,p2,&s_hashTable,this,p3);
}

Value CSSStyleRule::getInParents(GET_METHOD_ARGS) const
{
    if(CSSRule::hasProperty(p1,p2)) return CSSRule::get(p1,p2,p3);
    return Undefined();
}

bool CSSStyleRule::put(PUT_METHOD_ARGS)
{
    return lookupPut<CSSStyleRule>(p1,p2,p3,p4,&s_hashTable,this);
}

bool CSSStyleRule::putInParents(PUT_METHOD_ARGS)
{
    if(CSSRule::hasProperty(p1,p2)) {
        CSSRule::put(p1,p2,p3,p4);
        return true;
    }
    return false;
}

Object CSSStyleRule::prototype(ExecState *p1) const
{
    if(p1) return p1->interpreter()->builtinObjectPrototype();
    return Object::dynamicCast(Null());
}

ObjectImp *CSSStyleRule::bridge(ExecState *p1) const
{
    return new DOMRWBridge<CSSStyleRule>(p1,static_cast<CSSStyleRule::Private *>(d));
}

Value CSSStyleRule::cache(ExecState *p1) const
{
    return cacheDOMObject<CSSStyleRule>(p1,static_cast<CSSStyleRule::Private *>(d));
}

bool CSSStyleSheet::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&CSSStyleSheet::s_hashTable,p2);
    if(e) return true;
    Object proto = CSSStyleSheetProto::self(p1);
    if(proto.hasProperty(p1,p2)) return true;
    if(StyleSheet::hasProperty(p1,p2)) return true;
    return false;
}

Value CSSStyleSheet::get(GET_METHOD_ARGS) const
{
    return lookupGet<CSSStyleSheetProtoFunc,CSSStyleSheet>(p1,p2,&s_hashTable,this,p3);
}

CSSStyleSheet CSSStyleSheetProtoFunc::cast(KJS::ExecState *exec, const ObjectImp *p1) const
{
    { const DOMBridge<CSSStyleSheet> *test = dynamic_cast<const DOMBridge<CSSStyleSheet> * >(p1);
    if(test) return CSSStyleSheet(static_cast<CSSStyleSheet::Private *>(test->impl())); }
    if(!exec) return CSSStyleSheet::null;
    ScriptInterpreter *interpreter = static_cast<ScriptInterpreter *>(exec->interpreter());
    DocumentImpl *document = (interpreter ? interpreter->document() : 0);
    Ecma *engine = (document ? document->ecmaEngine() : 0);
    EcmaInterface *interface = (engine ? engine->interface() : 0);
    if(!interface) return CSSStyleSheet::null;
    return interface->inheritedCSSStyleSheetCast(p1);
}

CSSStyleSheet KDOM::EcmaInterface::inheritedCSSStyleSheetCast(const ObjectImp *p1)
{
	Q_UNUSED(p1); return CSSStyleSheet::null;
}

Value CSSStyleSheet::getInParents(GET_METHOD_ARGS) const
{
    Object proto = CSSStyleSheetProto::self(p1);
    if(proto.hasProperty(p1,p2)) return proto.get(p1,p2);
    if(StyleSheet::hasProperty(p1,p2)) return StyleSheet::get(p1,p2,p3);
    return Undefined();
}

bool CSSStyleSheet::put(PUT_METHOD_ARGS)
{
    return lookupPut<CSSStyleSheet>(p1,p2,p3,p4,&s_hashTable,this);
}

bool CSSStyleSheet::putInParents(PUT_METHOD_ARGS)
{
    if(StyleSheet::hasProperty(p1,p2)) {
        StyleSheet::put(p1,p2,p3,p4);
        return true;
    }
    return false;
}

Object CSSStyleSheet::prototype(ExecState *p1) const
{
    if(p1) return CSSStyleSheetProto::self(p1);
    return Object::dynamicCast(Null());
}

ObjectImp *CSSStyleSheet::bridge(ExecState *p1) const
{
    return new DOMRWBridge<CSSStyleSheet>(p1,static_cast<CSSStyleSheet::Private *>(d));
}

Value CSSStyleSheet::cache(ExecState *p1) const
{
    return cacheDOMObject<CSSStyleSheet>(p1,static_cast<CSSStyleSheet::Private *>(d));
}

bool CSSValue::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&CSSValue::s_hashTable,p2);
    if(e) return true;
    Q_UNUSED(p1);
    return false;
}

Value CSSValue::get(GET_METHOD_ARGS) const
{
    return lookupGetValue<CSSValue>(p1,p2,&s_hashTable,this,p3);
}

Value CSSValue::getInParents(GET_METHOD_ARGS) const
{
    Q_UNUSED(p1); Q_UNUSED(p2); Q_UNUSED(p3);
    return Undefined();
}

bool CSSValue::put(PUT_METHOD_ARGS)
{
    return lookupPut<CSSValue>(p1,p2,p3,p4,&s_hashTable,this);
}

bool CSSValue::putInParents(PUT_METHOD_ARGS)
{
    Q_UNUSED(p1); Q_UNUSED(p2); Q_UNUSED(p3); Q_UNUSED(p4);
    return false;
}

Object CSSValue::prototype(ExecState *p1) const
{
    if(p1) return p1->interpreter()->builtinObjectPrototype();
    return Object::dynamicCast(Null());
}

ObjectImp *CSSValue::bridge(ExecState *p1) const
{
    return new DOMRWBridge<CSSValue>(p1,static_cast<CSSValue::Private *>(d));
}

Value CSSValue::cache(ExecState *p1) const
{
    return cacheDOMObject<CSSValue>(p1,static_cast<CSSValue::Private *>(d));
}

bool CSSValueList::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&CSSValueList::s_hashTable,p2);
    if(e) return true;
    Object proto = CSSValueListProto::self(p1);
    if(proto.hasProperty(p1,p2)) return true;
    if(CSSValue::hasProperty(p1,p2)) return true;
    bool ok;
    unsigned int i = p2.toArrayIndex(&ok);
    if(ok && i < length()) return true;
    return false;
}

Value CSSValueList::get(GET_METHOD_ARGS) const
{
    bool ok;
    unsigned int i = p2.toArrayIndex(&ok);
    if(ok && i < length()) return item(i).cache(p1);
    return lookupGet<CSSValueListProtoFunc,CSSValueList>(p1,p2,&s_hashTable,this,p3);
}

CSSValueList CSSValueListProtoFunc::cast(KJS::ExecState *exec, const ObjectImp *p1) const
{
    { const DOMBridge<CSSValueList> *test = dynamic_cast<const DOMBridge<CSSValueList> * >(p1);
    if(test) return CSSValueList(static_cast<CSSValueList::Private *>(test->impl())); }
    if(!exec) return CSSValueList::null;
    ScriptInterpreter *interpreter = static_cast<ScriptInterpreter *>(exec->interpreter());
    DocumentImpl *document = (interpreter ? interpreter->document() : 0);
    Ecma *engine = (document ? document->ecmaEngine() : 0);
    EcmaInterface *interface = (engine ? engine->interface() : 0);
    if(!interface) return CSSValueList::null;
    return interface->inheritedCSSValueListCast(p1);
}

CSSValueList KDOM::EcmaInterface::inheritedCSSValueListCast(const ObjectImp *p1)
{
	Q_UNUSED(p1); return CSSValueList::null;
}

Value CSSValueList::getInParents(GET_METHOD_ARGS) const
{
    Object proto = CSSValueListProto::self(p1);
    if(proto.hasProperty(p1,p2)) return proto.get(p1,p2);
    if(CSSValue::hasProperty(p1,p2)) return CSSValue::get(p1,p2,p3);
    return Undefined();
}

bool CSSValueList::put(PUT_METHOD_ARGS)
{
    return lookupPut<CSSValueList>(p1,p2,p3,p4,&s_hashTable,this);
}

bool CSSValueList::putInParents(PUT_METHOD_ARGS)
{
    if(CSSValue::hasProperty(p1,p2)) {
        CSSValue::put(p1,p2,p3,p4);
        return true;
    }
    return false;
}

Object CSSValueList::prototype(ExecState *p1) const
{
    if(p1) return CSSValueListProto::self(p1);
    return Object::dynamicCast(Null());
}

ObjectImp *CSSValueList::bridge(ExecState *p1) const
{
    return new DOMRWBridge<CSSValueList>(p1,static_cast<CSSValueList::Private *>(d));
}

Value CSSValueList::cache(ExecState *p1) const
{
    return cacheDOMObject<CSSValueList>(p1,static_cast<CSSValueList::Private *>(d));
}

bool CharacterData::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&CharacterData::s_hashTable,p2);
    if(e) return true;
    Object proto = CharacterDataProto::self(p1);
    if(proto.hasProperty(p1,p2)) return true;
    if(Node::hasProperty(p1,p2)) return true;
    return false;
}

Value CharacterData::get(GET_METHOD_ARGS) const
{
    return lookupGet<CharacterDataProtoFunc,CharacterData>(p1,p2,&s_hashTable,this,p3);
}

CharacterData CharacterDataProtoFunc::cast(KJS::ExecState *exec, const ObjectImp *p1) const
{
    { const DOMBridge<CharacterData> *test = dynamic_cast<const DOMBridge<CharacterData> * >(p1);
    if(test) return CharacterData(static_cast<CharacterData::Private *>(test->impl())); }
    { const DOMBridge<CDATASection> *test = dynamic_cast<const DOMBridge<CDATASection> * >(p1);
    if(test) return CharacterData(static_cast<CharacterData::Private *>(test->impl())); }
    { const DOMBridge<Comment> *test = dynamic_cast<const DOMBridge<Comment> * >(p1);
    if(test) return CharacterData(static_cast<CharacterData::Private *>(test->impl())); }
    { const DOMBridge<Text> *test = dynamic_cast<const DOMBridge<Text> * >(p1);
    if(test) return CharacterData(static_cast<CharacterData::Private *>(test->impl())); }
    if(!exec) return CharacterData::null;
    ScriptInterpreter *interpreter = static_cast<ScriptInterpreter *>(exec->interpreter());
    DocumentImpl *document = (interpreter ? interpreter->document() : 0);
    Ecma *engine = (document ? document->ecmaEngine() : 0);
    EcmaInterface *interface = (engine ? engine->interface() : 0);
    if(!interface) return CharacterData::null;
    return interface->inheritedCharacterDataCast(p1);
}

CharacterData KDOM::EcmaInterface::inheritedCharacterDataCast(const ObjectImp *p1)
{
	Q_UNUSED(p1); return CharacterData::null;
}

Value CharacterData::getInParents(GET_METHOD_ARGS) const
{
    Object proto = CharacterDataProto::self(p1);
    if(proto.hasProperty(p1,p2)) return proto.get(p1,p2);
    if(Node::hasProperty(p1,p2)) return Node::get(p1,p2,p3);
    return Undefined();
}

bool CharacterData::put(PUT_METHOD_ARGS)
{
    return lookupPut<CharacterData>(p1,p2,p3,p4,&s_hashTable,this);
}

bool CharacterData::putInParents(PUT_METHOD_ARGS)
{
    if(Node::hasProperty(p1,p2)) {
        Node::put(p1,p2,p3,p4);
        return true;
    }
    return false;
}

Object CharacterData::prototype(ExecState *p1) const
{
    if(p1) return CharacterDataProto::self(p1);
    return Object::dynamicCast(Null());
}

ObjectImp *CharacterData::bridge(ExecState *p1) const
{
    return new DOMRWBridge<CharacterData>(p1,static_cast<CharacterData::Private *>(d));
}

Value CharacterData::cache(ExecState *p1) const
{
    return cacheDOMObject<CharacterData>(p1,static_cast<CharacterData::Private *>(d));
}

bool Counter::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&Counter::s_hashTable,p2);
    if(e) return true;
    Q_UNUSED(p1);
    return false;
}

Value Counter::get(GET_METHOD_ARGS) const
{
    return lookupGetValue<Counter>(p1,p2,&s_hashTable,this,p3);
}

Value Counter::getInParents(GET_METHOD_ARGS) const
{
    Q_UNUSED(p1); Q_UNUSED(p2); Q_UNUSED(p3);
    return Undefined();
}

Object Counter::prototype(ExecState *p1) const
{
    if(p1) return p1->interpreter()->builtinObjectPrototype();
    return Object::dynamicCast(Null());
}

ObjectImp *Counter::bridge(ExecState *p1) const
{
    return new DOMBridge<Counter>(p1,static_cast<Counter::Private *>(d));
}

Value Counter::cache(ExecState *p1) const
{
    return cacheDOMObject<Counter>(p1,static_cast<Counter::Private *>(d));
}

bool DOMConfiguration::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&DOMConfiguration::s_hashTable,p2);
    if(e) return true;
    Object proto = DOMConfigurationProto::self(p1);
    if(proto.hasProperty(p1,p2)) return true;
    return false;
}

Value DOMConfiguration::get(GET_METHOD_ARGS) const
{
    return lookupGet<DOMConfigurationProtoFunc,DOMConfiguration>(p1,p2,&s_hashTable,this,p3);
}

DOMConfiguration DOMConfigurationProtoFunc::cast(KJS::ExecState *exec, const ObjectImp *p1) const
{
    { const DOMBridge<DOMConfiguration> *test = dynamic_cast<const DOMBridge<DOMConfiguration> * >(p1);
    if(test) return DOMConfiguration(static_cast<DOMConfiguration::Private *>(test->impl())); }
    if(!exec) return DOMConfiguration::null;
    ScriptInterpreter *interpreter = static_cast<ScriptInterpreter *>(exec->interpreter());
    DocumentImpl *document = (interpreter ? interpreter->document() : 0);
    Ecma *engine = (document ? document->ecmaEngine() : 0);
    EcmaInterface *interface = (engine ? engine->interface() : 0);
    if(!interface) return DOMConfiguration::null;
    return interface->inheritedDOMConfigurationCast(p1);
}

DOMConfiguration KDOM::EcmaInterface::inheritedDOMConfigurationCast(const ObjectImp *p1)
{
	Q_UNUSED(p1); return DOMConfiguration::null;
}

Value DOMConfiguration::getInParents(GET_METHOD_ARGS) const
{
    Object proto = DOMConfigurationProto::self(p1);
    if(proto.hasProperty(p1,p2)) return proto.get(p1,p2);
    Q_UNUSED(p3);
    return Undefined();
}

Object DOMConfiguration::prototype(ExecState *p1) const
{
    if(p1) return DOMConfigurationProto::self(p1);
    return Object::dynamicCast(Null());
}

ObjectImp *DOMConfiguration::bridge(ExecState *p1) const
{
    return new DOMBridge<DOMConfiguration>(p1,static_cast<DOMConfiguration::Private *>(d));
}

Value DOMConfiguration::cache(ExecState *p1) const
{
    return cacheDOMObject<DOMConfiguration>(p1,static_cast<DOMConfiguration::Private *>(d));
}

bool DOMError::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&DOMError::s_hashTable,p2);
    if(e) return true;
    Q_UNUSED(p1);
    return false;
}

Value DOMError::get(GET_METHOD_ARGS) const
{
    return lookupGetValue<DOMError>(p1,p2,&s_hashTable,this,p3);
}

DOMError KDOM::toDOMError(KJS::ExecState *exec, const ObjectImp *p1)
{
	Q_UNUSED(exec);
    { const DOMBridge<DOMError> *test = dynamic_cast<const DOMBridge<DOMError> * >(p1);
    if(test) return DOMError(test->impl()); }
    return DOMError::null;
}

Value DOMError::getInParents(GET_METHOD_ARGS) const
{
    Q_UNUSED(p1); Q_UNUSED(p2); Q_UNUSED(p3);
    return Undefined();
}

Object DOMError::prototype(ExecState *p1) const
{
    if(p1) return p1->interpreter()->builtinObjectPrototype();
    return Object::dynamicCast(Null());
}

ObjectImp *DOMError::bridge(ExecState *p1) const
{
    return new DOMBridge<DOMError>(p1,static_cast<DOMError::Private *>(d));
}

Value DOMError::cache(ExecState *p1) const
{
    return cacheDOMObject<DOMError>(p1,static_cast<DOMError::Private *>(d));
}

bool DOMErrorHandler::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&DOMErrorHandler::s_hashTable,p2);
    if(e) return true;
    Object proto = DOMErrorHandlerProto::self(p1);
    if(proto.hasProperty(p1,p2)) return true;
    return false;
}

Value DOMErrorHandler::get(GET_METHOD_ARGS) const
{
    return lookupGet<DOMErrorHandlerProtoFunc,DOMErrorHandler>(p1,p2,&s_hashTable,this,p3);
}

DOMErrorHandler DOMErrorHandlerProtoFunc::cast(KJS::ExecState *exec, const ObjectImp *p1) const
{
    { const DOMBridge<DOMErrorHandler> *test = dynamic_cast<const DOMBridge<DOMErrorHandler> * >(p1);
    if(test) return DOMErrorHandler(static_cast<DOMErrorHandler::Private *>(test->impl())); }
    if(!exec) return DOMErrorHandler::null;
    ScriptInterpreter *interpreter = static_cast<ScriptInterpreter *>(exec->interpreter());
    DocumentImpl *document = (interpreter ? interpreter->document() : 0);
    Ecma *engine = (document ? document->ecmaEngine() : 0);
    EcmaInterface *interface = (engine ? engine->interface() : 0);
    if(!interface) return DOMErrorHandler::null;
    return interface->inheritedDOMErrorHandlerCast(p1);
}

DOMErrorHandler KDOM::EcmaInterface::inheritedDOMErrorHandlerCast(const ObjectImp *p1)
{
	Q_UNUSED(p1); return DOMErrorHandler::null;
}

Value DOMErrorHandler::getInParents(GET_METHOD_ARGS) const
{
    Object proto = DOMErrorHandlerProto::self(p1);
    if(proto.hasProperty(p1,p2)) return proto.get(p1,p2);
    Q_UNUSED(p3);
    return Undefined();
}

Object DOMErrorHandler::prototype(ExecState *p1) const
{
    if(p1) return DOMErrorHandlerProto::self(p1);
    return Object::dynamicCast(Null());
}

ObjectImp *DOMErrorHandler::bridge(ExecState *p1) const
{
    return new DOMBridge<DOMErrorHandler>(p1,static_cast<DOMErrorHandler::Private *>(d));
}

Value DOMErrorHandler::cache(ExecState *p1) const
{
    return cacheDOMObject<DOMErrorHandler>(p1,static_cast<DOMErrorHandler::Private *>(d));
}

bool DOMException::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&DOMException::s_hashTable,p2);
    if(e) return true;
    Q_UNUSED(p1);
    return false;
}

Value DOMException::get(GET_METHOD_ARGS) const
{
    return lookupGetValue<DOMException>(p1,p2,&s_hashTable,this,p3);
}

Value DOMException::getInParents(GET_METHOD_ARGS) const
{
    Q_UNUSED(p1); Q_UNUSED(p2); Q_UNUSED(p3);
    return Undefined();
}

Object DOMException::prototype(ExecState *p1) const
{
    if(p1) return p1->interpreter()->builtinObjectPrototype();
    return Object::dynamicCast(Null());
}

ObjectImp *DOMException::bridge(ExecState *p1) const
{
    return new DOMBridge<DOMException>(p1,static_cast<DOMException::Private *>(d));
}

Value DOMException::cache(ExecState *p1) const
{
    return cacheDOMObject<DOMException>(p1,static_cast<DOMException::Private *>(d));
}

bool DOMImplementation::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&DOMImplementation::s_hashTable,p2);
    if(e) return true;
    Object proto = DOMImplementationProto::self(p1);
    if(proto.hasProperty(p1,p2)) return true;
    return false;
}

Value DOMImplementation::get(GET_METHOD_ARGS) const
{
    return lookupGet<DOMImplementationProtoFunc,DOMImplementation>(p1,p2,&s_hashTable,this,p3);
}

DOMImplementation DOMImplementationProtoFunc::cast(KJS::ExecState *exec, const ObjectImp *p1) const
{
    { const DOMBridge<DOMImplementation> *test = dynamic_cast<const DOMBridge<DOMImplementation> * >(p1);
    if(test) return DOMImplementation(static_cast<DOMImplementation::Private *>(test->impl())); }
    if(!exec) return DOMImplementation::null;
    ScriptInterpreter *interpreter = static_cast<ScriptInterpreter *>(exec->interpreter());
    DocumentImpl *document = (interpreter ? interpreter->document() : 0);
    Ecma *engine = (document ? document->ecmaEngine() : 0);
    EcmaInterface *interface = (engine ? engine->interface() : 0);
    if(!interface) return DOMImplementation::null;
    return interface->inheritedDOMImplementationCast(p1);
}

DOMImplementation KDOM::EcmaInterface::inheritedDOMImplementationCast(const ObjectImp *p1)
{
	Q_UNUSED(p1); return DOMImplementation::null;
}

Value DOMImplementation::getInParents(GET_METHOD_ARGS) const
{
    Object proto = DOMImplementationProto::self(p1);
    if(proto.hasProperty(p1,p2)) return proto.get(p1,p2);
    Q_UNUSED(p3);
    return Undefined();
}

Object DOMImplementation::prototype(ExecState *p1) const
{
    if(p1) return DOMImplementationProto::self(p1);
    return Object::dynamicCast(Null());
}

ObjectImp *DOMImplementation::bridge(ExecState *p1) const
{
    return new DOMBridge<DOMImplementation>(p1,static_cast<DOMImplementation::Private *>(d));
}

Value DOMImplementation::cache(ExecState *p1) const
{
    return cacheDOMObject<DOMImplementation>(p1,static_cast<DOMImplementation::Private *>(d));
}

bool DOMLocator::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&DOMLocator::s_hashTable,p2);
    if(e) return true;
    Q_UNUSED(p1);
    return false;
}

Value DOMLocator::get(GET_METHOD_ARGS) const
{
    return lookupGetValue<DOMLocator>(p1,p2,&s_hashTable,this,p3);
}

Value DOMLocator::getInParents(GET_METHOD_ARGS) const
{
    Q_UNUSED(p1); Q_UNUSED(p2); Q_UNUSED(p3);
    return Undefined();
}

Object DOMLocator::prototype(ExecState *p1) const
{
    if(p1) return p1->interpreter()->builtinObjectPrototype();
    return Object::dynamicCast(Null());
}

ObjectImp *DOMLocator::bridge(ExecState *p1) const
{
    return new DOMBridge<DOMLocator>(p1,static_cast<DOMLocator::Private *>(d));
}

Value DOMLocator::cache(ExecState *p1) const
{
    return cacheDOMObject<DOMLocator>(p1,static_cast<DOMLocator::Private *>(d));
}

bool DOMStringList::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&DOMStringList::s_hashTable,p2);
    if(e) return true;
    Object proto = DOMStringListProto::self(p1);
    if(proto.hasProperty(p1,p2)) return true;
    return false;
}

Value DOMStringList::get(GET_METHOD_ARGS) const
{
    return lookupGet<DOMStringListProtoFunc,DOMStringList>(p1,p2,&s_hashTable,this,p3);
}

DOMStringList DOMStringListProtoFunc::cast(KJS::ExecState *exec, const ObjectImp *p1) const
{
    { const DOMBridge<DOMStringList> *test = dynamic_cast<const DOMBridge<DOMStringList> * >(p1);
    if(test) return DOMStringList(static_cast<DOMStringList::Private *>(test->impl())); }
    if(!exec) return DOMStringList::null;
    ScriptInterpreter *interpreter = static_cast<ScriptInterpreter *>(exec->interpreter());
    DocumentImpl *document = (interpreter ? interpreter->document() : 0);
    Ecma *engine = (document ? document->ecmaEngine() : 0);
    EcmaInterface *interface = (engine ? engine->interface() : 0);
    if(!interface) return DOMStringList::null;
    return interface->inheritedDOMStringListCast(p1);
}

DOMStringList KDOM::EcmaInterface::inheritedDOMStringListCast(const ObjectImp *p1)
{
	Q_UNUSED(p1); return DOMStringList::null;
}

Value DOMStringList::getInParents(GET_METHOD_ARGS) const
{
    Object proto = DOMStringListProto::self(p1);
    if(proto.hasProperty(p1,p2)) return proto.get(p1,p2);
    Q_UNUSED(p3);
    return Undefined();
}

Object DOMStringList::prototype(ExecState *p1) const
{
    if(p1) return DOMStringListProto::self(p1);
    return Object::dynamicCast(Null());
}

ObjectImp *DOMStringList::bridge(ExecState *p1) const
{
    return new DOMBridge<DOMStringList>(p1,static_cast<DOMStringList::Private *>(d));
}

Value DOMStringList::cache(ExecState *p1) const
{
    return cacheDOMObject<DOMStringList>(p1,static_cast<DOMStringList::Private *>(d));
}

bool DOMUserData::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&DOMUserData::s_hashTable,p2);
    if(e) return true;
    Q_UNUSED(p1);
    return false;
}

Value DOMUserData::get(GET_METHOD_ARGS) const
{
    return lookupGetValue<DOMUserData>(p1,p2,&s_hashTable,this,p3);
}

DOMUserData KDOM::toDOMUserData(KJS::ExecState *exec, const ObjectImp *p1)
{
	Q_UNUSED(exec);
    { const DOMBridge<DOMUserData> *test = dynamic_cast<const DOMBridge<DOMUserData> * >(p1);
    if(test) return DOMUserData(test->impl()); }
    return DOMUserData::null;
}

Value DOMUserData::getInParents(GET_METHOD_ARGS) const
{
    Q_UNUSED(p1); Q_UNUSED(p2); Q_UNUSED(p3);
    return Undefined();
}

Object DOMUserData::prototype(ExecState *p1) const
{
    if(p1) return p1->interpreter()->builtinObjectPrototype();
    return Object::dynamicCast(Null());
}

ObjectImp *DOMUserData::bridge(ExecState *p1) const
{
    return new DOMBridge<DOMUserData>(p1,static_cast<DOMUserData::Private *>(d));
}

Value DOMUserData::cache(ExecState *p1) const
{
    return cacheDOMObject<DOMUserData>(p1,static_cast<DOMUserData::Private *>(d));
}

bool Document::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&Document::s_hashTable,p2);
    if(e) return true;
    Object proto = DocumentProto::self(p1);
    if(proto.hasProperty(p1,p2)) return true;
    if(DocumentEvent::hasProperty(p1,p2)) return true;
    if(DocumentRange::hasProperty(p1,p2)) return true;
    if(DocumentStyle::hasProperty(p1,p2)) return true;
    if(DocumentTraversal::hasProperty(p1,p2)) return true;
    if(DocumentView::hasProperty(p1,p2)) return true;
    if(Node::hasProperty(p1,p2)) return true;
    if(XPathEvaluator::hasProperty(p1,p2)) return true;
    return false;
}

Value Document::get(GET_METHOD_ARGS) const
{
    return lookupGet<DocumentProtoFunc,Document>(p1,p2,&s_hashTable,this,p3);
}

Document DocumentProtoFunc::cast(KJS::ExecState *exec, const ObjectImp *p1) const
{
    { const DOMBridge<Document> *test = dynamic_cast<const DOMBridge<Document> * >(p1);
    if(test) return Document(static_cast<Document::Private *>(test->impl())); }
    if(!exec) return Document::null;
    ScriptInterpreter *interpreter = static_cast<ScriptInterpreter *>(exec->interpreter());
    DocumentImpl *document = (interpreter ? interpreter->document() : 0);
    Ecma *engine = (document ? document->ecmaEngine() : 0);
    EcmaInterface *interface = (engine ? engine->interface() : 0);
    if(!interface) return Document::null;
    return interface->inheritedDocumentCast(p1);
}

Document KDOM::EcmaInterface::inheritedDocumentCast(const ObjectImp *p1)
{
	Q_UNUSED(p1); return Document::null;
}

Value Document::getInParents(GET_METHOD_ARGS) const
{
    Object proto = DocumentProto::self(p1);
    if(proto.hasProperty(p1,p2)) return proto.get(p1,p2);
    if(DocumentEvent::hasProperty(p1,p2)) return DocumentEvent::get(p1,p2,p3);
    if(DocumentRange::hasProperty(p1,p2)) return DocumentRange::get(p1,p2,p3);
    if(DocumentStyle::hasProperty(p1,p2)) return DocumentStyle::get(p1,p2,p3);
    if(DocumentTraversal::hasProperty(p1,p2)) return DocumentTraversal::get(p1,p2,p3);
    if(DocumentView::hasProperty(p1,p2)) return DocumentView::get(p1,p2,p3);
    if(Node::hasProperty(p1,p2)) return Node::get(p1,p2,p3);
    if(XPathEvaluator::hasProperty(p1,p2)) return XPathEvaluator::get(p1,p2,p3);
    return Undefined();
}

bool Document::put(PUT_METHOD_ARGS)
{
    return lookupPut<Document>(p1,p2,p3,p4,&s_hashTable,this);
}

bool Document::putInParents(PUT_METHOD_ARGS)
{
    if(Node::hasProperty(p1,p2)) {
        Node::put(p1,p2,p3,p4);
        return true;
    }
    return false;
}

Object Document::prototype(ExecState *p1) const
{
    if(p1) return DocumentProto::self(p1);
    return Object::dynamicCast(Null());
}

ObjectImp *Document::bridge(ExecState *p1) const
{
    return new DOMRWBridge<Document>(p1,static_cast<Document::Private *>(KDOM::EventTarget::d));
}

Value Document::cache(ExecState *p1) const
{
    return cacheDOMObject<Document>(p1,static_cast<Document::Private *>(KDOM::EventTarget::d));
}

bool DocumentEvent::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&DocumentEvent::s_hashTable,p2);
    if(e) return true;
    Object proto = DocumentEventProto::self(p1);
    if(proto.hasProperty(p1,p2)) return true;
    return false;
}

Value DocumentEvent::get(GET_METHOD_ARGS) const
{
    return lookupGet<DocumentEventProtoFunc,DocumentEvent>(p1,p2,&s_hashTable,this,p3);
}

DocumentEvent DocumentEventProtoFunc::cast(KJS::ExecState *exec, const ObjectImp *p1) const
{
    { const DOMBridge<DocumentEvent> *test = dynamic_cast<const DOMBridge<DocumentEvent> * >(p1);
    if(test) return DocumentEvent(static_cast<DocumentEvent::Private *>(test->impl())); }
    { const DOMBridge<Document> *test = dynamic_cast<const DOMBridge<Document> * >(p1);
    if(test) return DocumentEvent(static_cast<DocumentEvent::Private *>(test->impl())); }
    if(!exec) return DocumentEvent::null;
    ScriptInterpreter *interpreter = static_cast<ScriptInterpreter *>(exec->interpreter());
    DocumentImpl *document = (interpreter ? interpreter->document() : 0);
    Ecma *engine = (document ? document->ecmaEngine() : 0);
    EcmaInterface *interface = (engine ? engine->interface() : 0);
    if(!interface) return DocumentEvent::null;
    return interface->inheritedDocumentEventCast(p1);
}

DocumentEvent KDOM::EcmaInterface::inheritedDocumentEventCast(const ObjectImp *p1)
{
	Q_UNUSED(p1); return DocumentEvent::null;
}

Value DocumentEvent::getInParents(GET_METHOD_ARGS) const
{
    Object proto = DocumentEventProto::self(p1);
    if(proto.hasProperty(p1,p2)) return proto.get(p1,p2);
    Q_UNUSED(p3);
    return Undefined();
}

Object DocumentEvent::prototype(ExecState *p1) const
{
    if(p1) return DocumentEventProto::self(p1);
    return Object::dynamicCast(Null());
}

ObjectImp *DocumentEvent::bridge(ExecState *p1) const
{
    return new DOMBridge<DocumentEvent>(p1,static_cast<DocumentEvent::Private *>(d));
}

Value DocumentEvent::cache(ExecState *p1) const
{
    return cacheDOMObject<DocumentEvent>(p1,static_cast<DocumentEvent::Private *>(d));
}

bool DocumentRange::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&DocumentRange::s_hashTable,p2);
    if(e) return true;
    Object proto = DocumentRangeProto::self(p1);
    if(proto.hasProperty(p1,p2)) return true;
    return false;
}

Value DocumentRange::get(GET_METHOD_ARGS) const
{
    return lookupGet<DocumentRangeProtoFunc,DocumentRange>(p1,p2,&s_hashTable,this,p3);
}

DocumentRange DocumentRangeProtoFunc::cast(KJS::ExecState *exec, const ObjectImp *p1) const
{
    { const DOMBridge<DocumentRange> *test = dynamic_cast<const DOMBridge<DocumentRange> * >(p1);
    if(test) return DocumentRange(static_cast<DocumentRange::Private *>(test->impl())); }
    { const DOMBridge<Document> *test = dynamic_cast<const DOMBridge<Document> * >(p1);
    if(test) return DocumentRange(static_cast<DocumentRange::Private *>(test->impl())); }
    if(!exec) return DocumentRange::null;
    ScriptInterpreter *interpreter = static_cast<ScriptInterpreter *>(exec->interpreter());
    DocumentImpl *document = (interpreter ? interpreter->document() : 0);
    Ecma *engine = (document ? document->ecmaEngine() : 0);
    EcmaInterface *interface = (engine ? engine->interface() : 0);
    if(!interface) return DocumentRange::null;
    return interface->inheritedDocumentRangeCast(p1);
}

DocumentRange KDOM::EcmaInterface::inheritedDocumentRangeCast(const ObjectImp *p1)
{
	Q_UNUSED(p1); return DocumentRange::null;
}

Value DocumentRange::getInParents(GET_METHOD_ARGS) const
{
    Object proto = DocumentRangeProto::self(p1);
    if(proto.hasProperty(p1,p2)) return proto.get(p1,p2);
    Q_UNUSED(p3);
    return Undefined();
}

Object DocumentRange::prototype(ExecState *p1) const
{
    if(p1) return DocumentRangeProto::self(p1);
    return Object::dynamicCast(Null());
}

ObjectImp *DocumentRange::bridge(ExecState *p1) const
{
    return new DOMBridge<DocumentRange>(p1,static_cast<DocumentRange::Private *>(d));
}

Value DocumentRange::cache(ExecState *p1) const
{
    return cacheDOMObject<DocumentRange>(p1,static_cast<DocumentRange::Private *>(d));
}

bool DocumentStyle::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&DocumentStyle::s_hashTable,p2);
    if(e) return true;
    Q_UNUSED(p1);
    return false;
}

Value DocumentStyle::get(GET_METHOD_ARGS) const
{
    return lookupGetValue<DocumentStyle>(p1,p2,&s_hashTable,this,p3);
}

Value DocumentStyle::getInParents(GET_METHOD_ARGS) const
{
    Q_UNUSED(p1); Q_UNUSED(p2); Q_UNUSED(p3);
    return Undefined();
}

Object DocumentStyle::prototype(ExecState *p1) const
{
    if(p1) return p1->interpreter()->builtinObjectPrototype();
    return Object::dynamicCast(Null());
}

ObjectImp *DocumentStyle::bridge(ExecState *p1) const
{
    return new DOMBridge<DocumentStyle>(p1,static_cast<DocumentStyle::Private *>(d));
}

Value DocumentStyle::cache(ExecState *p1) const
{
    return cacheDOMObject<DocumentStyle>(p1,static_cast<DocumentStyle::Private *>(d));
}

bool DocumentTraversal::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&DocumentTraversal::s_hashTable,p2);
    if(e) return true;
    Object proto = DocumentTraversalProto::self(p1);
    if(proto.hasProperty(p1,p2)) return true;
    return false;
}

Value DocumentTraversal::get(GET_METHOD_ARGS) const
{
    return lookupGet<DocumentTraversalProtoFunc,DocumentTraversal>(p1,p2,&s_hashTable,this,p3);
}

DocumentTraversal DocumentTraversalProtoFunc::cast(KJS::ExecState *exec, const ObjectImp *p1) const
{
    { const DOMBridge<DocumentTraversal> *test = dynamic_cast<const DOMBridge<DocumentTraversal> * >(p1);
    if(test) return DocumentTraversal(static_cast<DocumentTraversal::Private *>(test->impl())); }
    { const DOMBridge<Document> *test = dynamic_cast<const DOMBridge<Document> * >(p1);
    if(test) return DocumentTraversal(static_cast<DocumentTraversal::Private *>(test->impl())); }
    if(!exec) return DocumentTraversal::null;
    ScriptInterpreter *interpreter = static_cast<ScriptInterpreter *>(exec->interpreter());
    DocumentImpl *document = (interpreter ? interpreter->document() : 0);
    Ecma *engine = (document ? document->ecmaEngine() : 0);
    EcmaInterface *interface = (engine ? engine->interface() : 0);
    if(!interface) return DocumentTraversal::null;
    return interface->inheritedDocumentTraversalCast(p1);
}

DocumentTraversal KDOM::EcmaInterface::inheritedDocumentTraversalCast(const ObjectImp *p1)
{
	Q_UNUSED(p1); return DocumentTraversal::null;
}

Value DocumentTraversal::getInParents(GET_METHOD_ARGS) const
{
    Object proto = DocumentTraversalProto::self(p1);
    if(proto.hasProperty(p1,p2)) return proto.get(p1,p2);
    Q_UNUSED(p3);
    return Undefined();
}

Object DocumentTraversal::prototype(ExecState *p1) const
{
    if(p1) return DocumentTraversalProto::self(p1);
    return Object::dynamicCast(Null());
}

ObjectImp *DocumentTraversal::bridge(ExecState *p1) const
{
    return new DOMBridge<DocumentTraversal>(p1,static_cast<DocumentTraversal::Private *>(d));
}

Value DocumentTraversal::cache(ExecState *p1) const
{
    return cacheDOMObject<DocumentTraversal>(p1,static_cast<DocumentTraversal::Private *>(d));
}

bool DocumentType::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&DocumentType::s_hashTable,p2);
    if(e) return true;
    if(Node::hasProperty(p1,p2)) return true;
    return false;
}

Value DocumentType::get(GET_METHOD_ARGS) const
{
    return lookupGetValue<DocumentType>(p1,p2,&s_hashTable,this,p3);
}

DocumentType KDOM::toDocumentType(KJS::ExecState *exec, const ObjectImp *p1)
{
	Q_UNUSED(exec);
    { const DOMBridge<DocumentType> *test = dynamic_cast<const DOMBridge<DocumentType> * >(p1);
    if(test) return DocumentType(test->impl()); }
    return DocumentType::null;
}

Value DocumentType::getInParents(GET_METHOD_ARGS) const
{
    if(Node::hasProperty(p1,p2)) return Node::get(p1,p2,p3);
    return Undefined();
}

bool DocumentType::put(PUT_METHOD_ARGS)
{
    if(Node::hasProperty(p1,p2)) {
        Node::put(p1,p2,p3,p4);
        return true;
    }
    return false;
}

Object DocumentType::prototype(ExecState *p1) const
{
    if(p1) return p1->interpreter()->builtinObjectPrototype();
    return Object::dynamicCast(Null());
}

ObjectImp *DocumentType::bridge(ExecState *p1) const
{
    return new DOMRWBridge<DocumentType>(p1,static_cast<DocumentType::Private *>(d));
}

Value DocumentType::cache(ExecState *p1) const
{
    return cacheDOMObject<DocumentType>(p1,static_cast<DocumentType::Private *>(d));
}

bool DocumentView::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&DocumentView::s_hashTable,p2);
    if(e) return true;
    Q_UNUSED(p1);
    return false;
}

Value DocumentView::get(GET_METHOD_ARGS) const
{
    return lookupGetValue<DocumentView>(p1,p2,&s_hashTable,this,p3);
}

Value DocumentView::getInParents(GET_METHOD_ARGS) const
{
    Q_UNUSED(p1); Q_UNUSED(p2); Q_UNUSED(p3);
    return Undefined();
}

Object DocumentView::prototype(ExecState *p1) const
{
    if(p1) return p1->interpreter()->builtinObjectPrototype();
    return Object::dynamicCast(Null());
}

ObjectImp *DocumentView::bridge(ExecState *p1) const
{
    return new DOMBridge<DocumentView>(p1,static_cast<DocumentView::Private *>(d));
}

Value DocumentView::cache(ExecState *p1) const
{
    return cacheDOMObject<DocumentView>(p1,static_cast<DocumentView::Private *>(d));
}

bool Element::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&Element::s_hashTable,p2);
    if(e) return true;
    Object proto = ElementProto::self(p1);
    if(proto.hasProperty(p1,p2)) return true;
    if(Node::hasProperty(p1,p2)) return true;
    return false;
}

Value Element::get(GET_METHOD_ARGS) const
{
    return lookupGet<ElementProtoFunc,Element>(p1,p2,&s_hashTable,this,p3);
}

Element ElementProtoFunc::cast(KJS::ExecState *exec, const ObjectImp *p1) const
{
    { const DOMBridge<Element> *test = dynamic_cast<const DOMBridge<Element> * >(p1);
    if(test) return Element(static_cast<Element::Private *>(test->impl())); }
    if(!exec) return Element::null;
    ScriptInterpreter *interpreter = static_cast<ScriptInterpreter *>(exec->interpreter());
    DocumentImpl *document = (interpreter ? interpreter->document() : 0);
    Ecma *engine = (document ? document->ecmaEngine() : 0);
    EcmaInterface *interface = (engine ? engine->interface() : 0);
    if(!interface) return Element::null;
    return interface->inheritedElementCast(p1);
}

Element KDOM::EcmaInterface::inheritedElementCast(const ObjectImp *p1)
{
	Q_UNUSED(p1); return Element::null;
}

Value Element::getInParents(GET_METHOD_ARGS) const
{
    Object proto = ElementProto::self(p1);
    if(proto.hasProperty(p1,p2)) return proto.get(p1,p2);
    if(Node::hasProperty(p1,p2)) return Node::get(p1,p2,p3);
    return Undefined();
}

bool Element::put(PUT_METHOD_ARGS)
{
    if(Node::hasProperty(p1,p2)) {
        Node::put(p1,p2,p3,p4);
        return true;
    }
    return false;
}

Object Element::prototype(ExecState *p1) const
{
    if(p1) return ElementProto::self(p1);
    return Object::dynamicCast(Null());
}

ObjectImp *Element::bridge(ExecState *p1) const
{
    return new DOMRWBridge<Element>(p1,static_cast<Element::Private *>(d));
}

Value Element::cache(ExecState *p1) const
{
    return cacheDOMObject<Element>(p1,static_cast<Element::Private *>(d));
}

bool Entity::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&Entity::s_hashTable,p2);
    if(e) return true;
    if(Node::hasProperty(p1,p2)) return true;
    return false;
}

Value Entity::get(GET_METHOD_ARGS) const
{
    return lookupGetValue<Entity>(p1,p2,&s_hashTable,this,p3);
}

Value Entity::getInParents(GET_METHOD_ARGS) const
{
    if(Node::hasProperty(p1,p2)) return Node::get(p1,p2,p3);
    return Undefined();
}

bool Entity::put(PUT_METHOD_ARGS)
{
    if(Node::hasProperty(p1,p2)) {
        Node::put(p1,p2,p3,p4);
        return true;
    }
    return false;
}

Object Entity::prototype(ExecState *p1) const
{
    if(p1) return p1->interpreter()->builtinObjectPrototype();
    return Object::dynamicCast(Null());
}

ObjectImp *Entity::bridge(ExecState *p1) const
{
    return new DOMRWBridge<Entity>(p1,static_cast<Entity::Private *>(d));
}

Value Entity::cache(ExecState *p1) const
{
    return cacheDOMObject<Entity>(p1,static_cast<Entity::Private *>(d));
}

bool Event::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&Event::s_hashTable,p2);
    if(e) return true;
    Object proto = EventProto::self(p1);
    if(proto.hasProperty(p1,p2)) return true;
    return false;
}

Value Event::get(GET_METHOD_ARGS) const
{
    return lookupGet<EventProtoFunc,Event>(p1,p2,&s_hashTable,this,p3);
}

Event EventProtoFunc::cast(KJS::ExecState *exec, const ObjectImp *p1) const
{
    return KDOM::toEvent(exec, p1);
}

Event KDOM::toEvent(KJS::ExecState *exec, const ObjectImp *p1)
{
    { const DOMBridge<Event> *test = dynamic_cast<const DOMBridge<Event> * >(p1);
    if(test) return Event(static_cast<Event::Private *>(test->impl())); }
    { const DOMBridge<KeyboardEvent> *test = dynamic_cast<const DOMBridge<KeyboardEvent> * >(p1);
    if(test) return Event(static_cast<Event::Private *>(test->impl())); }
    { const DOMBridge<MouseEvent> *test = dynamic_cast<const DOMBridge<MouseEvent> * >(p1);
    if(test) return Event(static_cast<Event::Private *>(test->impl())); }
    { const DOMBridge<MutationEvent> *test = dynamic_cast<const DOMBridge<MutationEvent> * >(p1);
    if(test) return Event(static_cast<Event::Private *>(test->impl())); }
    { const DOMBridge<UIEvent> *test = dynamic_cast<const DOMBridge<UIEvent> * >(p1);
    if(test) return Event(static_cast<Event::Private *>(test->impl())); }
    if(!exec) return Event::null;
    ScriptInterpreter *interpreter = static_cast<ScriptInterpreter *>(exec->interpreter());
    DocumentImpl *document = (interpreter ? interpreter->document() : 0);
    Ecma *engine = (document ? document->ecmaEngine() : 0);
    EcmaInterface *interface = (engine ? engine->interface() : 0);
    if(!interface) return Event::null;
    return interface->inheritedEventCast(p1);
}

Event KDOM::EcmaInterface::inheritedEventCast(const ObjectImp *p1)
{
	Q_UNUSED(p1); return Event::null;
}

Value Event::getInParents(GET_METHOD_ARGS) const
{
    Object proto = EventProto::self(p1);
    if(proto.hasProperty(p1,p2)) return proto.get(p1,p2);
    Q_UNUSED(p3);
    return Undefined();
}

Object Event::prototype(ExecState *p1) const
{
    if(p1) return EventProto::self(p1);
    return Object::dynamicCast(Null());
}

ObjectImp *Event::bridge(ExecState *p1) const
{
    return new DOMBridge<Event>(p1,static_cast<Event::Private *>(d));
}

Value Event::cache(ExecState *p1) const
{
    return cacheDOMObject<Event>(p1,static_cast<Event::Private *>(d));
}

bool EventException::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&EventException::s_hashTable,p2);
    if(e) return true;
    Q_UNUSED(p1);
    return false;
}

Value EventException::get(GET_METHOD_ARGS) const
{
    return lookupGetValue<EventException>(p1,p2,&s_hashTable,this,p3);
}

Value EventException::getInParents(GET_METHOD_ARGS) const
{
    Q_UNUSED(p1); Q_UNUSED(p2); Q_UNUSED(p3);
    return Undefined();
}

Object EventException::prototype(ExecState *p1) const
{
    if(p1) return p1->interpreter()->builtinObjectPrototype();
    return Object::dynamicCast(Null());
}

ObjectImp *EventException::bridge(ExecState *p1) const
{
    return new DOMBridge<EventException>(p1,static_cast<EventException::Private *>(d));
}

Value EventException::cache(ExecState *p1) const
{
    return cacheDOMObject<EventException>(p1,static_cast<EventException::Private *>(d));
}

bool EventTarget::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&EventTarget::s_hashTable,p2);
    if(e) return true;
    Object proto = EventTargetProto::self(p1);
    if(proto.hasProperty(p1,p2)) return true;
    return false;
}

Value EventTarget::get(GET_METHOD_ARGS) const
{
    return lookupGet<EventTargetProtoFunc,EventTarget>(p1,p2,&s_hashTable,this,p3);
}

EventTarget EventTargetProtoFunc::cast(KJS::ExecState *exec, const ObjectImp *p1) const
{
    { const DOMBridge<EventTarget> *test = dynamic_cast<const DOMBridge<EventTarget> * >(p1);
    if(test) return EventTarget(static_cast<EventTarget::Private *>(test->impl())); }
    { const DOMBridge<Attr> *test = dynamic_cast<const DOMBridge<Attr> * >(p1);
    if(test) return EventTarget(static_cast<EventTarget::Private *>(test->impl())); }
    { const DOMBridge<CDATASection> *test = dynamic_cast<const DOMBridge<CDATASection> * >(p1);
    if(test) return EventTarget(static_cast<EventTarget::Private *>(test->impl())); }
    { const DOMBridge<CharacterData> *test = dynamic_cast<const DOMBridge<CharacterData> * >(p1);
    if(test) return EventTarget(static_cast<EventTarget::Private *>(test->impl())); }
    { const DOMBridge<Comment> *test = dynamic_cast<const DOMBridge<Comment> * >(p1);
    if(test) return EventTarget(static_cast<EventTarget::Private *>(test->impl())); }
    { const DOMBridge<Document> *test = dynamic_cast<const DOMBridge<Document> * >(p1);
    if(test) return EventTarget(static_cast<EventTarget::Private *>(test->impl())); }
    { const DOMBridge<DocumentFragment> *test = dynamic_cast<const DOMBridge<DocumentFragment> * >(p1);
    if(test) return EventTarget(static_cast<EventTarget::Private *>(test->impl())); }
    { const DOMBridge<DocumentType> *test = dynamic_cast<const DOMBridge<DocumentType> * >(p1);
    if(test) return EventTarget(static_cast<EventTarget::Private *>(test->impl())); }
    { const DOMBridge<Element> *test = dynamic_cast<const DOMBridge<Element> * >(p1);
    if(test) return EventTarget(static_cast<EventTarget::Private *>(test->impl())); }
    { const DOMBridge<Entity> *test = dynamic_cast<const DOMBridge<Entity> * >(p1);
    if(test) return EventTarget(static_cast<EventTarget::Private *>(test->impl())); }
    { const DOMBridge<EntityReference> *test = dynamic_cast<const DOMBridge<EntityReference> * >(p1);
    if(test) return EventTarget(static_cast<EventTarget::Private *>(test->impl())); }
    { const DOMBridge<LSParser> *test = dynamic_cast<const DOMBridge<LSParser> * >(p1);
    if(test) return EventTarget(static_cast<EventTarget::Private *>(test->impl())); }
    { const DOMBridge<Node> *test = dynamic_cast<const DOMBridge<Node> * >(p1);
    if(test) return EventTarget(static_cast<EventTarget::Private *>(test->impl())); }
    { const DOMBridge<Notation> *test = dynamic_cast<const DOMBridge<Notation> * >(p1);
    if(test) return EventTarget(static_cast<EventTarget::Private *>(test->impl())); }
    { const DOMBridge<ProcessingInstruction> *test = dynamic_cast<const DOMBridge<ProcessingInstruction> * >(p1);
    if(test) return EventTarget(static_cast<EventTarget::Private *>(test->impl())); }
    { const DOMBridge<Text> *test = dynamic_cast<const DOMBridge<Text> * >(p1);
    if(test) return EventTarget(static_cast<EventTarget::Private *>(test->impl())); }
    { const DOMBridge<XPathNamespace> *test = dynamic_cast<const DOMBridge<XPathNamespace> * >(p1);
    if(test) return EventTarget(static_cast<EventTarget::Private *>(test->impl())); }
    if(!exec) return EventTarget::null;
    ScriptInterpreter *interpreter = static_cast<ScriptInterpreter *>(exec->interpreter());
    DocumentImpl *document = (interpreter ? interpreter->document() : 0);
    Ecma *engine = (document ? document->ecmaEngine() : 0);
    EcmaInterface *interface = (engine ? engine->interface() : 0);
    if(!interface) return EventTarget::null;
    return interface->inheritedEventTargetCast(p1);
}

EventTarget KDOM::EcmaInterface::inheritedEventTargetCast(const ObjectImp *p1)
{
	Q_UNUSED(p1); return EventTarget::null;
}

Value EventTarget::getInParents(GET_METHOD_ARGS) const
{
    Object proto = EventTargetProto::self(p1);
    if(proto.hasProperty(p1,p2)) return proto.get(p1,p2);
    Q_UNUSED(p3);
    return Undefined();
}

bool EventTarget::put(PUT_METHOD_ARGS)
{
    Q_UNUSED(p1); Q_UNUSED(p2); Q_UNUSED(p3); Q_UNUSED(p4);
    return false;
}

Object EventTarget::prototype(ExecState *p1) const
{
    if(p1) return EventTargetProto::self(p1);
    return Object::dynamicCast(Null());
}

ObjectImp *EventTarget::bridge(ExecState *p1) const
{
    return new DOMRWBridge<EventTarget>(p1,static_cast<EventTarget::Private *>(d));
}

Value EventTarget::cache(ExecState *p1) const
{
    return cacheDOMObject<EventTarget>(p1,static_cast<EventTarget::Private *>(d));
}

bool KeyboardEvent::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&KeyboardEvent::s_hashTable,p2);
    if(e) return true;
    Object proto = KeyboardEventProto::self(p1);
    if(proto.hasProperty(p1,p2)) return true;
    if(UIEvent::hasProperty(p1,p2)) return true;
    return false;
}

Value KeyboardEvent::get(GET_METHOD_ARGS) const
{
    return lookupGet<KeyboardEventProtoFunc,KeyboardEvent>(p1,p2,&s_hashTable,this,p3);
}

KeyboardEvent KeyboardEventProtoFunc::cast(KJS::ExecState *exec, const ObjectImp *p1) const
{
    { const DOMBridge<KeyboardEvent> *test = dynamic_cast<const DOMBridge<KeyboardEvent> * >(p1);
    if(test) return KeyboardEvent(static_cast<KeyboardEvent::Private *>(test->impl())); }
    if(!exec) return KeyboardEvent::null;
    ScriptInterpreter *interpreter = static_cast<ScriptInterpreter *>(exec->interpreter());
    DocumentImpl *document = (interpreter ? interpreter->document() : 0);
    Ecma *engine = (document ? document->ecmaEngine() : 0);
    EcmaInterface *interface = (engine ? engine->interface() : 0);
    if(!interface) return KeyboardEvent::null;
    return interface->inheritedKeyboardEventCast(p1);
}

KeyboardEvent KDOM::EcmaInterface::inheritedKeyboardEventCast(const ObjectImp *p1)
{
	Q_UNUSED(p1); return KeyboardEvent::null;
}

Value KeyboardEvent::getInParents(GET_METHOD_ARGS) const
{
    Object proto = KeyboardEventProto::self(p1);
    if(proto.hasProperty(p1,p2)) return proto.get(p1,p2);
    if(UIEvent::hasProperty(p1,p2)) return UIEvent::get(p1,p2,p3);
    return Undefined();
}

Object KeyboardEvent::prototype(ExecState *p1) const
{
    if(p1) return KeyboardEventProto::self(p1);
    return Object::dynamicCast(Null());
}

ObjectImp *KeyboardEvent::bridge(ExecState *p1) const
{
    return new DOMBridge<KeyboardEvent>(p1,static_cast<KeyboardEvent::Private *>(d));
}

Value KeyboardEvent::cache(ExecState *p1) const
{
    return cacheDOMObject<KeyboardEvent>(p1,static_cast<KeyboardEvent::Private *>(d));
}

bool LSException::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&LSException::s_hashTable,p2);
    if(e) return true;
    Q_UNUSED(p1);
    return false;
}

Value LSException::get(GET_METHOD_ARGS) const
{
    return lookupGetValue<LSException>(p1,p2,&s_hashTable,this,p3);
}

Value LSException::getInParents(GET_METHOD_ARGS) const
{
    Q_UNUSED(p1); Q_UNUSED(p2); Q_UNUSED(p3);
    return Undefined();
}

Object LSException::prototype(ExecState *p1) const
{
    if(p1) return p1->interpreter()->builtinObjectPrototype();
    return Object::dynamicCast(Null());
}

ObjectImp *LSException::bridge(ExecState *p1) const
{
    return new DOMBridge<LSException>(p1,static_cast<LSException::Private *>(d));
}

Value LSException::cache(ExecState *p1) const
{
    return cacheDOMObject<LSException>(p1,static_cast<LSException::Private *>(d));
}

bool LSInput::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&LSInput::s_hashTable,p2);
    if(e) return true;
    Q_UNUSED(p1);
    return false;
}

Value LSInput::get(GET_METHOD_ARGS) const
{
    return lookupGetValue<LSInput>(p1,p2,&s_hashTable,this,p3);
}

LSInput KDOM::toLSInput(KJS::ExecState *exec, const ObjectImp *p1)
{
	Q_UNUSED(exec);
    { const DOMBridge<LSInput> *test = dynamic_cast<const DOMBridge<LSInput> * >(p1);
    if(test) return LSInput(test->impl()); }
    return LSInput::null;
}

Value LSInput::getInParents(GET_METHOD_ARGS) const
{
    Q_UNUSED(p1); Q_UNUSED(p2); Q_UNUSED(p3);
    return Undefined();
}

bool LSInput::put(PUT_METHOD_ARGS)
{
    return lookupPut<LSInput>(p1,p2,p3,p4,&s_hashTable,this);
}

bool LSInput::putInParents(PUT_METHOD_ARGS)
{
    Q_UNUSED(p1); Q_UNUSED(p2); Q_UNUSED(p3); Q_UNUSED(p4);
    return false;
}

Object LSInput::prototype(ExecState *p1) const
{
    if(p1) return p1->interpreter()->builtinObjectPrototype();
    return Object::dynamicCast(Null());
}

ObjectImp *LSInput::bridge(ExecState *p1) const
{
    return new DOMRWBridge<LSInput>(p1,static_cast<LSInput::Private *>(d));
}

Value LSInput::cache(ExecState *p1) const
{
    return cacheDOMObject<LSInput>(p1,static_cast<LSInput::Private *>(d));
}

bool LSOutput::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&LSOutput::s_hashTable,p2);
    if(e) return true;
    Q_UNUSED(p1);
    return false;
}

Value LSOutput::get(GET_METHOD_ARGS) const
{
    return lookupGetValue<LSOutput>(p1,p2,&s_hashTable,this,p3);
}

LSOutput KDOM::toLSOutput(KJS::ExecState *exec, const ObjectImp *p1)
{
	Q_UNUSED(exec);
    { const DOMBridge<LSOutput> *test = dynamic_cast<const DOMBridge<LSOutput> * >(p1);
    if(test) return LSOutput(test->impl()); }
    return LSOutput::null;
}

Value LSOutput::getInParents(GET_METHOD_ARGS) const
{
    Q_UNUSED(p1); Q_UNUSED(p2); Q_UNUSED(p3);
    return Undefined();
}

bool LSOutput::put(PUT_METHOD_ARGS)
{
    return lookupPut<LSOutput>(p1,p2,p3,p4,&s_hashTable,this);
}

bool LSOutput::putInParents(PUT_METHOD_ARGS)
{
    Q_UNUSED(p1); Q_UNUSED(p2); Q_UNUSED(p3); Q_UNUSED(p4);
    return false;
}

Object LSOutput::prototype(ExecState *p1) const
{
    if(p1) return p1->interpreter()->builtinObjectPrototype();
    return Object::dynamicCast(Null());
}

ObjectImp *LSOutput::bridge(ExecState *p1) const
{
    return new DOMRWBridge<LSOutput>(p1,static_cast<LSOutput::Private *>(d));
}

Value LSOutput::cache(ExecState *p1) const
{
    return cacheDOMObject<LSOutput>(p1,static_cast<LSOutput::Private *>(d));
}

bool LSParser::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&LSParser::s_hashTable,p2);
    if(e) return true;
    Object proto = LSParserProto::self(p1);
    if(proto.hasProperty(p1,p2)) return true;
    if(EventTarget::hasProperty(p1,p2)) return true;
    return false;
}

Value LSParser::get(GET_METHOD_ARGS) const
{
    return lookupGet<LSParserProtoFunc,LSParser>(p1,p2,&s_hashTable,this,p3);
}

LSParser LSParserProtoFunc::cast(KJS::ExecState *exec, const ObjectImp *p1) const
{
    { const DOMBridge<LSParser> *test = dynamic_cast<const DOMBridge<LSParser> * >(p1);
    if(test) return LSParser(static_cast<LSParser::Private *>(test->impl())); }
    if(!exec) return LSParser::null;
    ScriptInterpreter *interpreter = static_cast<ScriptInterpreter *>(exec->interpreter());
    DocumentImpl *document = (interpreter ? interpreter->document() : 0);
    Ecma *engine = (document ? document->ecmaEngine() : 0);
    EcmaInterface *interface = (engine ? engine->interface() : 0);
    if(!interface) return LSParser::null;
    return interface->inheritedLSParserCast(p1);
}

LSParser KDOM::EcmaInterface::inheritedLSParserCast(const ObjectImp *p1)
{
	Q_UNUSED(p1); return LSParser::null;
}

Value LSParser::getInParents(GET_METHOD_ARGS) const
{
    Object proto = LSParserProto::self(p1);
    if(proto.hasProperty(p1,p2)) return proto.get(p1,p2);
    if(EventTarget::hasProperty(p1,p2)) return EventTarget::get(p1,p2,p3);
    return Undefined();
}

bool LSParser::put(PUT_METHOD_ARGS)
{
    return lookupPut<LSParser>(p1,p2,p3,p4,&s_hashTable,this);
}

bool LSParser::putInParents(PUT_METHOD_ARGS)
{
    if(EventTarget::hasProperty(p1,p2)) {
        EventTarget::put(p1,p2,p3,p4);
        return true;
    }
    return false;
}

Object LSParser::prototype(ExecState *p1) const
{
    if(p1) return LSParserProto::self(p1);
    return Object::dynamicCast(Null());
}

ObjectImp *LSParser::bridge(ExecState *p1) const
{
    return new DOMRWBridge<LSParser>(p1,static_cast<LSParser::Private *>(d));
}

Value LSParser::cache(ExecState *p1) const
{
    return cacheDOMObject<LSParser>(p1,static_cast<LSParser::Private *>(d));
}

bool LSParserFilter::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&LSParserFilter::s_hashTable,p2);
    if(e) return true;
    Object proto = LSParserFilterProto::self(p1);
    if(proto.hasProperty(p1,p2)) return true;
    return false;
}

Value LSParserFilter::get(GET_METHOD_ARGS) const
{
    return lookupGet<LSParserFilterProtoFunc,LSParserFilter>(p1,p2,&s_hashTable,this,p3);
}

LSParserFilter LSParserFilterProtoFunc::cast(KJS::ExecState *exec, const ObjectImp *p1) const
{
    { const DOMBridge<LSParserFilter> *test = dynamic_cast<const DOMBridge<LSParserFilter> * >(p1);
    if(test) return LSParserFilter(static_cast<LSParserFilter::Private *>(test->impl())); }
    if(!exec) return LSParserFilter::null;
    ScriptInterpreter *interpreter = static_cast<ScriptInterpreter *>(exec->interpreter());
    DocumentImpl *document = (interpreter ? interpreter->document() : 0);
    Ecma *engine = (document ? document->ecmaEngine() : 0);
    EcmaInterface *interface = (engine ? engine->interface() : 0);
    if(!interface) return LSParserFilter::null;
    return interface->inheritedLSParserFilterCast(p1);
}

LSParserFilter KDOM::EcmaInterface::inheritedLSParserFilterCast(const ObjectImp *p1)
{
	Q_UNUSED(p1); return LSParserFilter::null;
}

Value LSParserFilter::getInParents(GET_METHOD_ARGS) const
{
    Object proto = LSParserFilterProto::self(p1);
    if(proto.hasProperty(p1,p2)) return proto.get(p1,p2);
    Q_UNUSED(p3);
    return Undefined();
}

Object LSParserFilter::prototype(ExecState *p1) const
{
    if(p1) return LSParserFilterProto::self(p1);
    return Object::dynamicCast(Null());
}

ObjectImp *LSParserFilter::bridge(ExecState *p1) const
{
    return new DOMBridge<LSParserFilter>(p1,static_cast<LSParserFilter::Private *>(d));
}

Value LSParserFilter::cache(ExecState *p1) const
{
    return cacheDOMObject<LSParserFilter>(p1,static_cast<LSParserFilter::Private *>(d));
}

bool LSResourceResolver::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&LSResourceResolver::s_hashTable,p2);
    if(e) return true;
    Object proto = LSResourceResolverProto::self(p1);
    if(proto.hasProperty(p1,p2)) return true;
    return false;
}

Value LSResourceResolver::get(GET_METHOD_ARGS) const
{
    return lookupGet<LSResourceResolverProtoFunc,LSResourceResolver>(p1,p2,&s_hashTable,this,p3);
}

LSResourceResolver LSResourceResolverProtoFunc::cast(KJS::ExecState *exec, const ObjectImp *p1) const
{
    { const DOMBridge<LSResourceResolver> *test = dynamic_cast<const DOMBridge<LSResourceResolver> * >(p1);
    if(test) return LSResourceResolver(static_cast<LSResourceResolver::Private *>(test->impl())); }
    if(!exec) return LSResourceResolver::null;
    ScriptInterpreter *interpreter = static_cast<ScriptInterpreter *>(exec->interpreter());
    DocumentImpl *document = (interpreter ? interpreter->document() : 0);
    Ecma *engine = (document ? document->ecmaEngine() : 0);
    EcmaInterface *interface = (engine ? engine->interface() : 0);
    if(!interface) return LSResourceResolver::null;
    return interface->inheritedLSResourceResolverCast(p1);
}

LSResourceResolver KDOM::EcmaInterface::inheritedLSResourceResolverCast(const ObjectImp *p1)
{
	Q_UNUSED(p1); return LSResourceResolver::null;
}

Value LSResourceResolver::getInParents(GET_METHOD_ARGS) const
{
    Object proto = LSResourceResolverProto::self(p1);
    if(proto.hasProperty(p1,p2)) return proto.get(p1,p2);
    Q_UNUSED(p3);
    return Undefined();
}

Object LSResourceResolver::prototype(ExecState *p1) const
{
    if(p1) return LSResourceResolverProto::self(p1);
    return Object::dynamicCast(Null());
}

ObjectImp *LSResourceResolver::bridge(ExecState *p1) const
{
    return new DOMBridge<LSResourceResolver>(p1,static_cast<LSResourceResolver::Private *>(d));
}

Value LSResourceResolver::cache(ExecState *p1) const
{
    return cacheDOMObject<LSResourceResolver>(p1,static_cast<LSResourceResolver::Private *>(d));
}

bool LSSerializer::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&LSSerializer::s_hashTable,p2);
    if(e) return true;
    Object proto = LSSerializerProto::self(p1);
    if(proto.hasProperty(p1,p2)) return true;
    return false;
}

Value LSSerializer::get(GET_METHOD_ARGS) const
{
    return lookupGet<LSSerializerProtoFunc,LSSerializer>(p1,p2,&s_hashTable,this,p3);
}

LSSerializer LSSerializerProtoFunc::cast(KJS::ExecState *exec, const ObjectImp *p1) const
{
    { const DOMBridge<LSSerializer> *test = dynamic_cast<const DOMBridge<LSSerializer> * >(p1);
    if(test) return LSSerializer(static_cast<LSSerializer::Private *>(test->impl())); }
    if(!exec) return LSSerializer::null;
    ScriptInterpreter *interpreter = static_cast<ScriptInterpreter *>(exec->interpreter());
    DocumentImpl *document = (interpreter ? interpreter->document() : 0);
    Ecma *engine = (document ? document->ecmaEngine() : 0);
    EcmaInterface *interface = (engine ? engine->interface() : 0);
    if(!interface) return LSSerializer::null;
    return interface->inheritedLSSerializerCast(p1);
}

LSSerializer KDOM::EcmaInterface::inheritedLSSerializerCast(const ObjectImp *p1)
{
	Q_UNUSED(p1); return LSSerializer::null;
}

Value LSSerializer::getInParents(GET_METHOD_ARGS) const
{
    Object proto = LSSerializerProto::self(p1);
    if(proto.hasProperty(p1,p2)) return proto.get(p1,p2);
    Q_UNUSED(p3);
    return Undefined();
}

bool LSSerializer::put(PUT_METHOD_ARGS)
{
    return lookupPut<LSSerializer>(p1,p2,p3,p4,&s_hashTable,this);
}

bool LSSerializer::putInParents(PUT_METHOD_ARGS)
{
    Q_UNUSED(p1); Q_UNUSED(p2); Q_UNUSED(p3); Q_UNUSED(p4);
    return false;
}

Object LSSerializer::prototype(ExecState *p1) const
{
    if(p1) return LSSerializerProto::self(p1);
    return Object::dynamicCast(Null());
}

ObjectImp *LSSerializer::bridge(ExecState *p1) const
{
    return new DOMRWBridge<LSSerializer>(p1,static_cast<LSSerializer::Private *>(d));
}

Value LSSerializer::cache(ExecState *p1) const
{
    return cacheDOMObject<LSSerializer>(p1,static_cast<LSSerializer::Private *>(d));
}

bool LSSerializerFilter::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&LSSerializerFilter::s_hashTable,p2);
    if(e) return true;
    Q_UNUSED(p1);
    return false;
}

Value LSSerializerFilter::get(GET_METHOD_ARGS) const
{
    return lookupGetValue<LSSerializerFilter>(p1,p2,&s_hashTable,this,p3);
}

Value LSSerializerFilter::getInParents(GET_METHOD_ARGS) const
{
    Q_UNUSED(p1); Q_UNUSED(p2); Q_UNUSED(p3);
    return Undefined();
}

Object LSSerializerFilter::prototype(ExecState *p1) const
{
    if(p1) return p1->interpreter()->builtinObjectPrototype();
    return Object::dynamicCast(Null());
}

ObjectImp *LSSerializerFilter::bridge(ExecState *p1) const
{
    return new DOMBridge<LSSerializerFilter>(p1,static_cast<LSSerializerFilter::Private *>(d));
}

Value LSSerializerFilter::cache(ExecState *p1) const
{
    return cacheDOMObject<LSSerializerFilter>(p1,static_cast<LSSerializerFilter::Private *>(d));
}

bool MediaList::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&MediaList::s_hashTable,p2);
    if(e) return true;
    Object proto = MediaListProto::self(p1);
    if(proto.hasProperty(p1,p2)) return true;
    bool ok;
    unsigned int i = p2.toArrayIndex(&ok);
    if(ok && i < length()) return true;
    return false;
}

Value MediaList::get(GET_METHOD_ARGS) const
{
    bool ok;
    unsigned int i = p2.toArrayIndex(&ok);
    if(ok && i < length()) return getDOMString(item(i));
    return lookupGet<MediaListProtoFunc,MediaList>(p1,p2,&s_hashTable,this,p3);
}

MediaList MediaListProtoFunc::cast(KJS::ExecState *exec, const ObjectImp *p1) const
{
    { const DOMBridge<MediaList> *test = dynamic_cast<const DOMBridge<MediaList> * >(p1);
    if(test) return MediaList(static_cast<MediaList::Private *>(test->impl())); }
    if(!exec) return MediaList::null;
    ScriptInterpreter *interpreter = static_cast<ScriptInterpreter *>(exec->interpreter());
    DocumentImpl *document = (interpreter ? interpreter->document() : 0);
    Ecma *engine = (document ? document->ecmaEngine() : 0);
    EcmaInterface *interface = (engine ? engine->interface() : 0);
    if(!interface) return MediaList::null;
    return interface->inheritedMediaListCast(p1);
}

MediaList KDOM::EcmaInterface::inheritedMediaListCast(const ObjectImp *p1)
{
	Q_UNUSED(p1); return MediaList::null;
}

Value MediaList::getInParents(GET_METHOD_ARGS) const
{
    Object proto = MediaListProto::self(p1);
    if(proto.hasProperty(p1,p2)) return proto.get(p1,p2);
    Q_UNUSED(p3);
    return Undefined();
}

bool MediaList::put(PUT_METHOD_ARGS)
{
    return lookupPut<MediaList>(p1,p2,p3,p4,&s_hashTable,this);
}

bool MediaList::putInParents(PUT_METHOD_ARGS)
{
    Q_UNUSED(p1); Q_UNUSED(p2); Q_UNUSED(p3); Q_UNUSED(p4);
    return false;
}

Object MediaList::prototype(ExecState *p1) const
{
    if(p1) return MediaListProto::self(p1);
    return Object::dynamicCast(Null());
}

ObjectImp *MediaList::bridge(ExecState *p1) const
{
    return new DOMRWBridge<MediaList>(p1,static_cast<MediaList::Private *>(d));
}

Value MediaList::cache(ExecState *p1) const
{
    return cacheDOMObject<MediaList>(p1,static_cast<MediaList::Private *>(d));
}

bool MouseEvent::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&MouseEvent::s_hashTable,p2);
    if(e) return true;
    Object proto = MouseEventProto::self(p1);
    if(proto.hasProperty(p1,p2)) return true;
    if(UIEvent::hasProperty(p1,p2)) return true;
    return false;
}

Value MouseEvent::get(GET_METHOD_ARGS) const
{
    return lookupGet<MouseEventProtoFunc,MouseEvent>(p1,p2,&s_hashTable,this,p3);
}

MouseEvent MouseEventProtoFunc::cast(KJS::ExecState *exec, const ObjectImp *p1) const
{
    { const DOMBridge<MouseEvent> *test = dynamic_cast<const DOMBridge<MouseEvent> * >(p1);
    if(test) return MouseEvent(static_cast<MouseEvent::Private *>(test->impl())); }
    if(!exec) return MouseEvent::null;
    ScriptInterpreter *interpreter = static_cast<ScriptInterpreter *>(exec->interpreter());
    DocumentImpl *document = (interpreter ? interpreter->document() : 0);
    Ecma *engine = (document ? document->ecmaEngine() : 0);
    EcmaInterface *interface = (engine ? engine->interface() : 0);
    if(!interface) return MouseEvent::null;
    return interface->inheritedMouseEventCast(p1);
}

MouseEvent KDOM::EcmaInterface::inheritedMouseEventCast(const ObjectImp *p1)
{
	Q_UNUSED(p1); return MouseEvent::null;
}

Value MouseEvent::getInParents(GET_METHOD_ARGS) const
{
    Object proto = MouseEventProto::self(p1);
    if(proto.hasProperty(p1,p2)) return proto.get(p1,p2);
    if(UIEvent::hasProperty(p1,p2)) return UIEvent::get(p1,p2,p3);
    return Undefined();
}

Object MouseEvent::prototype(ExecState *p1) const
{
    if(p1) return MouseEventProto::self(p1);
    return Object::dynamicCast(Null());
}

ObjectImp *MouseEvent::bridge(ExecState *p1) const
{
    return new DOMBridge<MouseEvent>(p1,static_cast<MouseEvent::Private *>(d));
}

Value MouseEvent::cache(ExecState *p1) const
{
    return cacheDOMObject<MouseEvent>(p1,static_cast<MouseEvent::Private *>(d));
}

bool MutationEvent::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&MutationEvent::s_hashTable,p2);
    if(e) return true;
    Object proto = MutationEventProto::self(p1);
    if(proto.hasProperty(p1,p2)) return true;
    if(Event::hasProperty(p1,p2)) return true;
    return false;
}

Value MutationEvent::get(GET_METHOD_ARGS) const
{
    return lookupGet<MutationEventProtoFunc,MutationEvent>(p1,p2,&s_hashTable,this,p3);
}

MutationEvent MutationEventProtoFunc::cast(KJS::ExecState *exec, const ObjectImp *p1) const
{
    { const DOMBridge<MutationEvent> *test = dynamic_cast<const DOMBridge<MutationEvent> * >(p1);
    if(test) return MutationEvent(static_cast<MutationEvent::Private *>(test->impl())); }
    if(!exec) return MutationEvent::null;
    ScriptInterpreter *interpreter = static_cast<ScriptInterpreter *>(exec->interpreter());
    DocumentImpl *document = (interpreter ? interpreter->document() : 0);
    Ecma *engine = (document ? document->ecmaEngine() : 0);
    EcmaInterface *interface = (engine ? engine->interface() : 0);
    if(!interface) return MutationEvent::null;
    return interface->inheritedMutationEventCast(p1);
}

MutationEvent KDOM::EcmaInterface::inheritedMutationEventCast(const ObjectImp *p1)
{
	Q_UNUSED(p1); return MutationEvent::null;
}

Value MutationEvent::getInParents(GET_METHOD_ARGS) const
{
    Object proto = MutationEventProto::self(p1);
    if(proto.hasProperty(p1,p2)) return proto.get(p1,p2);
    if(Event::hasProperty(p1,p2)) return Event::get(p1,p2,p3);
    return Undefined();
}

Object MutationEvent::prototype(ExecState *p1) const
{
    if(p1) return MutationEventProto::self(p1);
    return Object::dynamicCast(Null());
}

ObjectImp *MutationEvent::bridge(ExecState *p1) const
{
    return new DOMBridge<MutationEvent>(p1,static_cast<MutationEvent::Private *>(d));
}

Value MutationEvent::cache(ExecState *p1) const
{
    return cacheDOMObject<MutationEvent>(p1,static_cast<MutationEvent::Private *>(d));
}

bool NamedNodeMap::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&NamedNodeMap::s_hashTable,p2);
    if(e) return true;
    Object proto = NamedNodeMapProto::self(p1);
    if(proto.hasProperty(p1,p2)) return true;
    bool ok;
    unsigned int i = p2.toArrayIndex(&ok);
    if(ok && i < length()) return true;
    return false;
}

Value NamedNodeMap::get(GET_METHOD_ARGS) const
{
    bool ok;
    unsigned int i = p2.toArrayIndex(&ok);
    if(ok && i < length()) return item(i).cache(p1);
    return lookupGet<NamedNodeMapProtoFunc,NamedNodeMap>(p1,p2,&s_hashTable,this,p3);
}

NamedNodeMap NamedNodeMapProtoFunc::cast(KJS::ExecState *exec, const ObjectImp *p1) const
{
    { const DOMBridge<NamedNodeMap> *test = dynamic_cast<const DOMBridge<NamedNodeMap> * >(p1);
    if(test) return NamedNodeMap(static_cast<NamedNodeMap::Private *>(test->impl())); }
    if(!exec) return NamedNodeMap::null;
    ScriptInterpreter *interpreter = static_cast<ScriptInterpreter *>(exec->interpreter());
    DocumentImpl *document = (interpreter ? interpreter->document() : 0);
    Ecma *engine = (document ? document->ecmaEngine() : 0);
    EcmaInterface *interface = (engine ? engine->interface() : 0);
    if(!interface) return NamedNodeMap::null;
    return interface->inheritedNamedNodeMapCast(p1);
}

NamedNodeMap KDOM::EcmaInterface::inheritedNamedNodeMapCast(const ObjectImp *p1)
{
	Q_UNUSED(p1); return NamedNodeMap::null;
}

Value NamedNodeMap::getInParents(GET_METHOD_ARGS) const
{
    Object proto = NamedNodeMapProto::self(p1);
    if(proto.hasProperty(p1,p2)) return proto.get(p1,p2);
    Q_UNUSED(p3);
    return Undefined();
}

bool NamedNodeMap::put(PUT_METHOD_ARGS)
{
    Q_UNUSED(p1); Q_UNUSED(p2); Q_UNUSED(p3); Q_UNUSED(p4);
    return false;
}

Object NamedNodeMap::prototype(ExecState *p1) const
{
    if(p1) return NamedNodeMapProto::self(p1);
    return Object::dynamicCast(Null());
}

ObjectImp *NamedNodeMap::bridge(ExecState *p1) const
{
    return new DOMRWBridge<NamedNodeMap>(p1,static_cast<NamedNodeMap::Private *>(d));
}

Value NamedNodeMap::cache(ExecState *p1) const
{
    return cacheDOMObject<NamedNodeMap>(p1,static_cast<NamedNodeMap::Private *>(d));
}

bool Node::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&Node::s_hashTable,p2);
    if(e) return true;
    Object proto = NodeProto::self(p1);
    if(proto.hasProperty(p1,p2)) return true;
    if(EventTarget::hasProperty(p1,p2)) return true;
    return false;
}

Value Node::get(GET_METHOD_ARGS) const
{
    return lookupGet<NodeProtoFunc,Node>(p1,p2,&s_hashTable,this,p3);
}

Node NodeProtoFunc::cast(KJS::ExecState *exec, const ObjectImp *p1) const
{
    return KDOM::toNode(exec, p1);
}

Node KDOM::toNode(KJS::ExecState *exec, const ObjectImp *p1)
{
    { const DOMBridge<Node> *test = dynamic_cast<const DOMBridge<Node> * >(p1);
    if(test) return Node(static_cast<Node::Private *>(test->impl())); }
    { const DOMBridge<Attr> *test = dynamic_cast<const DOMBridge<Attr> * >(p1);
    if(test) return Node(static_cast<Node::Private *>(test->impl())); }
    { const DOMBridge<CDATASection> *test = dynamic_cast<const DOMBridge<CDATASection> * >(p1);
    if(test) return Node(static_cast<Node::Private *>(test->impl())); }
    { const DOMBridge<CharacterData> *test = dynamic_cast<const DOMBridge<CharacterData> * >(p1);
    if(test) return Node(static_cast<Node::Private *>(test->impl())); }
    { const DOMBridge<Comment> *test = dynamic_cast<const DOMBridge<Comment> * >(p1);
    if(test) return Node(static_cast<Node::Private *>(test->impl())); }
    { const DOMBridge<Document> *test = dynamic_cast<const DOMBridge<Document> * >(p1);
    if(test) return Node(static_cast<Node::Private *>(test->impl())); }
    { const DOMBridge<DocumentFragment> *test = dynamic_cast<const DOMBridge<DocumentFragment> * >(p1);
    if(test) return Node(static_cast<Node::Private *>(test->impl())); }
    { const DOMBridge<DocumentType> *test = dynamic_cast<const DOMBridge<DocumentType> * >(p1);
    if(test) return Node(static_cast<Node::Private *>(test->impl())); }
    { const DOMBridge<Element> *test = dynamic_cast<const DOMBridge<Element> * >(p1);
    if(test) return Node(static_cast<Node::Private *>(test->impl())); }
    { const DOMBridge<Entity> *test = dynamic_cast<const DOMBridge<Entity> * >(p1);
    if(test) return Node(static_cast<Node::Private *>(test->impl())); }
    { const DOMBridge<EntityReference> *test = dynamic_cast<const DOMBridge<EntityReference> * >(p1);
    if(test) return Node(static_cast<Node::Private *>(test->impl())); }
    { const DOMBridge<Notation> *test = dynamic_cast<const DOMBridge<Notation> * >(p1);
    if(test) return Node(static_cast<Node::Private *>(test->impl())); }
    { const DOMBridge<ProcessingInstruction> *test = dynamic_cast<const DOMBridge<ProcessingInstruction> * >(p1);
    if(test) return Node(static_cast<Node::Private *>(test->impl())); }
    { const DOMBridge<Text> *test = dynamic_cast<const DOMBridge<Text> * >(p1);
    if(test) return Node(static_cast<Node::Private *>(test->impl())); }
    { const DOMBridge<XPathNamespace> *test = dynamic_cast<const DOMBridge<XPathNamespace> * >(p1);
    if(test) return Node(static_cast<Node::Private *>(test->impl())); }
    if(!exec) return Node::null;
    ScriptInterpreter *interpreter = static_cast<ScriptInterpreter *>(exec->interpreter());
    DocumentImpl *document = (interpreter ? interpreter->document() : 0);
    Ecma *engine = (document ? document->ecmaEngine() : 0);
    EcmaInterface *interface = (engine ? engine->interface() : 0);
    if(!interface) return Node::null;
    return interface->inheritedNodeCast(p1);
}

Node KDOM::EcmaInterface::inheritedNodeCast(const ObjectImp *p1)
{
	Q_UNUSED(p1); return Node::null;
}

Value Node::getInParents(GET_METHOD_ARGS) const
{
    Object proto = NodeProto::self(p1);
    if(proto.hasProperty(p1,p2)) return proto.get(p1,p2);
    if(EventTarget::hasProperty(p1,p2)) return EventTarget::get(p1,p2,p3);
    return Undefined();
}

bool Node::put(PUT_METHOD_ARGS)
{
    return lookupPut<Node>(p1,p2,p3,p4,&s_hashTable,this);
}

bool Node::putInParents(PUT_METHOD_ARGS)
{
    if(EventTarget::hasProperty(p1,p2)) {
        EventTarget::put(p1,p2,p3,p4);
        return true;
    }
    return false;
}

Object Node::prototype(ExecState *p1) const
{
    if(p1) return NodeProto::self(p1);
    return Object::dynamicCast(Null());
}

ObjectImp *Node::bridge(ExecState *p1) const
{
    return new DOMRWBridge<Node>(p1,static_cast<Node::Private *>(d));
}

Value Node::cache(ExecState *p1) const
{
    return cacheDOMObject<Node>(p1,static_cast<Node::Private *>(d));
}

bool NodeIterator::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&NodeIterator::s_hashTable,p2);
    if(e) return true;
    Object proto = NodeIteratorProto::self(p1);
    if(proto.hasProperty(p1,p2)) return true;
    return false;
}

Value NodeIterator::get(GET_METHOD_ARGS) const
{
    return lookupGet<NodeIteratorProtoFunc,NodeIterator>(p1,p2,&s_hashTable,this,p3);
}

NodeIterator NodeIteratorProtoFunc::cast(KJS::ExecState *exec, const ObjectImp *p1) const
{
    { const DOMBridge<NodeIterator> *test = dynamic_cast<const DOMBridge<NodeIterator> * >(p1);
    if(test) return NodeIterator(static_cast<NodeIterator::Private *>(test->impl())); }
    if(!exec) return NodeIterator::null;
    ScriptInterpreter *interpreter = static_cast<ScriptInterpreter *>(exec->interpreter());
    DocumentImpl *document = (interpreter ? interpreter->document() : 0);
    Ecma *engine = (document ? document->ecmaEngine() : 0);
    EcmaInterface *interface = (engine ? engine->interface() : 0);
    if(!interface) return NodeIterator::null;
    return interface->inheritedNodeIteratorCast(p1);
}

NodeIterator KDOM::EcmaInterface::inheritedNodeIteratorCast(const ObjectImp *p1)
{
	Q_UNUSED(p1); return NodeIterator::null;
}

Value NodeIterator::getInParents(GET_METHOD_ARGS) const
{
    Object proto = NodeIteratorProto::self(p1);
    if(proto.hasProperty(p1,p2)) return proto.get(p1,p2);
    Q_UNUSED(p3);
    return Undefined();
}

Object NodeIterator::prototype(ExecState *p1) const
{
    if(p1) return NodeIteratorProto::self(p1);
    return Object::dynamicCast(Null());
}

ObjectImp *NodeIterator::bridge(ExecState *p1) const
{
    return new DOMBridge<NodeIterator>(p1,static_cast<NodeIterator::Private *>(d));
}

Value NodeIterator::cache(ExecState *p1) const
{
    return cacheDOMObject<NodeIterator>(p1,static_cast<NodeIterator::Private *>(d));
}

bool NodeList::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&NodeList::s_hashTable,p2);
    if(e) return true;
    Object proto = NodeListProto::self(p1);
    if(proto.hasProperty(p1,p2)) return true;
    bool ok;
    unsigned int i = p2.toArrayIndex(&ok);
    if(ok && i < length()) return true;
    return false;
}

Value NodeList::get(GET_METHOD_ARGS) const
{
    bool ok;
    unsigned int i = p2.toArrayIndex(&ok);
    if(ok && i < length()) return getDOMNode(p1, item(i));
    return lookupGet<NodeListProtoFunc,NodeList>(p1,p2,&s_hashTable,this,p3);
}

NodeList NodeListProtoFunc::cast(KJS::ExecState *exec, const ObjectImp *p1) const
{
    { const DOMBridge<NodeList> *test = dynamic_cast<const DOMBridge<NodeList> * >(p1);
    if(test) return NodeList(static_cast<NodeList::Private *>(test->impl())); }
    if(!exec) return NodeList::null;
    ScriptInterpreter *interpreter = static_cast<ScriptInterpreter *>(exec->interpreter());
    DocumentImpl *document = (interpreter ? interpreter->document() : 0);
    Ecma *engine = (document ? document->ecmaEngine() : 0);
    EcmaInterface *interface = (engine ? engine->interface() : 0);
    if(!interface) return NodeList::null;
    return interface->inheritedNodeListCast(p1);
}

NodeList KDOM::EcmaInterface::inheritedNodeListCast(const ObjectImp *p1)
{
	Q_UNUSED(p1); return NodeList::null;
}

Value NodeList::getInParents(GET_METHOD_ARGS) const
{
    Object proto = NodeListProto::self(p1);
    if(proto.hasProperty(p1,p2)) return proto.get(p1,p2);
    Q_UNUSED(p3);
    return Undefined();
}

Object NodeList::prototype(ExecState *p1) const
{
    if(p1) return NodeListProto::self(p1);
    return Object::dynamicCast(Null());
}

ObjectImp *NodeList::bridge(ExecState *p1) const
{
    return new DOMBridge<NodeList>(p1,static_cast<NodeList::Private *>(d));
}

Value NodeList::cache(ExecState *p1) const
{
    return cacheDOMObject<NodeList>(p1,static_cast<NodeList::Private *>(d));
}

bool Notation::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&Notation::s_hashTable,p2);
    if(e) return true;
    if(Node::hasProperty(p1,p2)) return true;
    return false;
}

Value Notation::get(GET_METHOD_ARGS) const
{
    return lookupGetValue<Notation>(p1,p2,&s_hashTable,this,p3);
}

Value Notation::getInParents(GET_METHOD_ARGS) const
{
    if(Node::hasProperty(p1,p2)) return Node::get(p1,p2,p3);
    return Undefined();
}

bool Notation::put(PUT_METHOD_ARGS)
{
    if(Node::hasProperty(p1,p2)) {
        Node::put(p1,p2,p3,p4);
        return true;
    }
    return false;
}

Object Notation::prototype(ExecState *p1) const
{
    if(p1) return p1->interpreter()->builtinObjectPrototype();
    return Object::dynamicCast(Null());
}

ObjectImp *Notation::bridge(ExecState *p1) const
{
    return new DOMRWBridge<Notation>(p1,static_cast<Notation::Private *>(d));
}

Value Notation::cache(ExecState *p1) const
{
    return cacheDOMObject<Notation>(p1,static_cast<Notation::Private *>(d));
}

bool ProcessingInstruction::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&ProcessingInstruction::s_hashTable,p2);
    if(e) return true;
    if(Node::hasProperty(p1,p2)) return true;
    return false;
}

Value ProcessingInstruction::get(GET_METHOD_ARGS) const
{
    return lookupGetValue<ProcessingInstruction>(p1,p2,&s_hashTable,this,p3);
}

Value ProcessingInstruction::getInParents(GET_METHOD_ARGS) const
{
    if(Node::hasProperty(p1,p2)) return Node::get(p1,p2,p3);
    return Undefined();
}

bool ProcessingInstruction::put(PUT_METHOD_ARGS)
{
    return lookupPut<ProcessingInstruction>(p1,p2,p3,p4,&s_hashTable,this);
}

bool ProcessingInstruction::putInParents(PUT_METHOD_ARGS)
{
    if(Node::hasProperty(p1,p2)) {
        Node::put(p1,p2,p3,p4);
        return true;
    }
    return false;
}

Object ProcessingInstruction::prototype(ExecState *p1) const
{
    if(p1) return p1->interpreter()->builtinObjectPrototype();
    return Object::dynamicCast(Null());
}

ObjectImp *ProcessingInstruction::bridge(ExecState *p1) const
{
    return new DOMRWBridge<ProcessingInstruction>(p1,static_cast<ProcessingInstruction::Private *>(d));
}

Value ProcessingInstruction::cache(ExecState *p1) const
{
    return cacheDOMObject<ProcessingInstruction>(p1,static_cast<ProcessingInstruction::Private *>(d));
}

bool KDOM::RGBColor::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&KDOM::RGBColor::s_hashTable,p2);
    if(e) return true;
    Q_UNUSED(p1);
    return false;
}

Value KDOM::RGBColor::get(GET_METHOD_ARGS) const
{
    return lookupGetValue<KDOM::RGBColor>(p1,p2,&s_hashTable,this,p3);
}

Value KDOM::RGBColor::getInParents(GET_METHOD_ARGS) const
{
    Q_UNUSED(p1); Q_UNUSED(p2); Q_UNUSED(p3);
    return Undefined();
}

Object KDOM::RGBColor::prototype(ExecState *p1) const
{
    if(p1) return p1->interpreter()->builtinObjectPrototype();
    return Object::dynamicCast(Null());
}

ObjectImp *KDOM::RGBColor::bridge(ExecState *p1) const
{
    return new DOMBridge<KDOM::RGBColor>(p1,static_cast<KDOM::RGBColor::Private *>(d));
}

Value KDOM::RGBColor::cache(ExecState *p1) const
{
    return cacheDOMObject<KDOM::RGBColor>(p1,static_cast<KDOM::RGBColor::Private *>(d));
}

bool Range::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&Range::s_hashTable,p2);
    if(e) return true;
    Object proto = RangeProto::self(p1);
    if(proto.hasProperty(p1,p2)) return true;
    return false;
}

Value Range::get(GET_METHOD_ARGS) const
{
    return lookupGet<RangeProtoFunc,Range>(p1,p2,&s_hashTable,this,p3);
}

Range RangeProtoFunc::cast(KJS::ExecState *exec, const ObjectImp *p1) const
{
    return KDOM::toRange(exec, p1);
}

Range KDOM::toRange(KJS::ExecState *exec, const ObjectImp *p1)
{
    { const DOMBridge<Range> *test = dynamic_cast<const DOMBridge<Range> * >(p1);
    if(test) return Range(static_cast<Range::Private *>(test->impl())); }
    if(!exec) return Range::null;
    ScriptInterpreter *interpreter = static_cast<ScriptInterpreter *>(exec->interpreter());
    DocumentImpl *document = (interpreter ? interpreter->document() : 0);
    Ecma *engine = (document ? document->ecmaEngine() : 0);
    EcmaInterface *interface = (engine ? engine->interface() : 0);
    if(!interface) return Range::null;
    return interface->inheritedRangeCast(p1);
}

Range KDOM::EcmaInterface::inheritedRangeCast(const ObjectImp *p1)
{
	Q_UNUSED(p1); return Range::null;
}

Value Range::getInParents(GET_METHOD_ARGS) const
{
    Object proto = RangeProto::self(p1);
    if(proto.hasProperty(p1,p2)) return proto.get(p1,p2);
    Q_UNUSED(p3);
    return Undefined();
}

Object Range::prototype(ExecState *p1) const
{
    if(p1) return RangeProto::self(p1);
    return Object::dynamicCast(Null());
}

ObjectImp *Range::bridge(ExecState *p1) const
{
    return new DOMBridge<Range>(p1,static_cast<Range::Private *>(d));
}

Value Range::cache(ExecState *p1) const
{
    return cacheDOMObject<Range>(p1,static_cast<Range::Private *>(d));
}

bool RangeException::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&RangeException::s_hashTable,p2);
    if(e) return true;
    Q_UNUSED(p1);
    return false;
}

Value RangeException::get(GET_METHOD_ARGS) const
{
    return lookupGetValue<RangeException>(p1,p2,&s_hashTable,this,p3);
}

Value RangeException::getInParents(GET_METHOD_ARGS) const
{
    Q_UNUSED(p1); Q_UNUSED(p2); Q_UNUSED(p3);
    return Undefined();
}

Object RangeException::prototype(ExecState *p1) const
{
    if(p1) return p1->interpreter()->builtinObjectPrototype();
    return Object::dynamicCast(Null());
}

ObjectImp *RangeException::bridge(ExecState *p1) const
{
    return new DOMBridge<RangeException>(p1,static_cast<RangeException::Private *>(d));
}

Value RangeException::cache(ExecState *p1) const
{
    return cacheDOMObject<RangeException>(p1,static_cast<RangeException::Private *>(d));
}

bool KDOM::Rect::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&KDOM::Rect::s_hashTable,p2);
    if(e) return true;
    Q_UNUSED(p1);
    return false;
}

Value KDOM::Rect::get(GET_METHOD_ARGS) const
{
    return lookupGetValue<Rect>(p1,p2,&s_hashTable,this,p3);
}

Value KDOM::Rect::getInParents(GET_METHOD_ARGS) const
{
    Q_UNUSED(p1); Q_UNUSED(p2); Q_UNUSED(p3);
    return Undefined();
}

Object KDOM::Rect::prototype(ExecState *p1) const
{
    if(p1) return p1->interpreter()->builtinObjectPrototype();
    return Object::dynamicCast(Null());
}

ObjectImp *KDOM::Rect::bridge(ExecState *p1) const
{
    return new DOMBridge<KDOM::Rect>(p1,static_cast<KDOM::Rect::Private *>(d));
}

Value KDOM::Rect::cache(ExecState *p1) const
{
    return cacheDOMObject<KDOM::Rect>(p1,static_cast<KDOM::Rect::Private *>(d));
}

bool StyleSheet::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&StyleSheet::s_hashTable,p2);
    if(e) return true;
    Q_UNUSED(p1);
    return false;
}

Value StyleSheet::get(GET_METHOD_ARGS) const
{
    return lookupGetValue<StyleSheet>(p1,p2,&s_hashTable,this,p3);
}

Value StyleSheet::getInParents(GET_METHOD_ARGS) const
{
    Q_UNUSED(p1); Q_UNUSED(p2); Q_UNUSED(p3);
    return Undefined();
}

bool StyleSheet::put(PUT_METHOD_ARGS)
{
    return lookupPut<StyleSheet>(p1,p2,p3,p4,&s_hashTable,this);
}

bool StyleSheet::putInParents(PUT_METHOD_ARGS)
{
    Q_UNUSED(p1); Q_UNUSED(p2); Q_UNUSED(p3); Q_UNUSED(p4);
    return false;
}

Object StyleSheet::prototype(ExecState *p1) const
{
    if(p1) return p1->interpreter()->builtinObjectPrototype();
    return Object::dynamicCast(Null());
}

ObjectImp *StyleSheet::bridge(ExecState *p1) const
{
    return new DOMRWBridge<StyleSheet>(p1,static_cast<StyleSheet::Private *>(d));
}

Value StyleSheet::cache(ExecState *p1) const
{
    return cacheDOMObject<StyleSheet>(p1,static_cast<StyleSheet::Private *>(d));
}

bool StyleSheetList::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&StyleSheetList::s_hashTable,p2);
    if(e) return true;
    Object proto = StyleSheetListProto::self(p1);
    if(proto.hasProperty(p1,p2)) return true;
    bool ok;
    unsigned int i = p2.toArrayIndex(&ok);
    if(ok && i < length()) return true;
    return false;
}

Value StyleSheetList::get(GET_METHOD_ARGS) const
{
    bool ok;
    unsigned int i = p2.toArrayIndex(&ok);
    if(ok && i < length()) return item(i).cache(p1);
    return lookupGet<StyleSheetListProtoFunc,StyleSheetList>(p1,p2,&s_hashTable,this,p3);
}

StyleSheetList StyleSheetListProtoFunc::cast(KJS::ExecState *exec, const ObjectImp *p1) const
{
    { const DOMBridge<StyleSheetList> *test = dynamic_cast<const DOMBridge<StyleSheetList> * >(p1);
    if(test) return StyleSheetList(static_cast<StyleSheetList::Private *>(test->impl())); }
    if(!exec) return StyleSheetList::null;
    ScriptInterpreter *interpreter = static_cast<ScriptInterpreter *>(exec->interpreter());
    DocumentImpl *document = (interpreter ? interpreter->document() : 0);
    Ecma *engine = (document ? document->ecmaEngine() : 0);
    EcmaInterface *interface = (engine ? engine->interface() : 0);
    if(!interface) return StyleSheetList::null;
    return interface->inheritedStyleSheetListCast(p1);
}

StyleSheetList KDOM::EcmaInterface::inheritedStyleSheetListCast(const ObjectImp *p1)
{
	Q_UNUSED(p1); return StyleSheetList::null;
}

Value StyleSheetList::getInParents(GET_METHOD_ARGS) const
{
    Object proto = StyleSheetListProto::self(p1);
    if(proto.hasProperty(p1,p2)) return proto.get(p1,p2);
    Q_UNUSED(p3);
    return Undefined();
}

Object StyleSheetList::prototype(ExecState *p1) const
{
    if(p1) return StyleSheetListProto::self(p1);
    return Object::dynamicCast(Null());
}

ObjectImp *StyleSheetList::bridge(ExecState *p1) const
{
    return new DOMBridge<StyleSheetList>(p1,static_cast<StyleSheetList::Private *>(d));
}

Value StyleSheetList::cache(ExecState *p1) const
{
    return cacheDOMObject<StyleSheetList>(p1,static_cast<StyleSheetList::Private *>(d));
}

bool Text::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&Text::s_hashTable,p2);
    if(e) return true;
    Object proto = TextProto::self(p1);
    if(proto.hasProperty(p1,p2)) return true;
    if(CharacterData::hasProperty(p1,p2)) return true;
    return false;
}

Value Text::get(GET_METHOD_ARGS) const
{
    return lookupGet<TextProtoFunc,Text>(p1,p2,&s_hashTable,this,p3);
}

Text TextProtoFunc::cast(KJS::ExecState *exec, const ObjectImp *p1) const
{
    { const DOMBridge<Text> *test = dynamic_cast<const DOMBridge<Text> * >(p1);
    if(test) return Text(static_cast<Text::Private *>(test->impl())); }
    { const DOMBridge<CDATASection> *test = dynamic_cast<const DOMBridge<CDATASection> * >(p1);
    if(test) return Text(static_cast<Text::Private *>(test->impl())); }
    if(!exec) return Text::null;
    ScriptInterpreter *interpreter = static_cast<ScriptInterpreter *>(exec->interpreter());
    DocumentImpl *document = (interpreter ? interpreter->document() : 0);
    Ecma *engine = (document ? document->ecmaEngine() : 0);
    EcmaInterface *interface = (engine ? engine->interface() : 0);
    if(!interface) return Text::null;
    return interface->inheritedTextCast(p1);
}

Text KDOM::EcmaInterface::inheritedTextCast(const ObjectImp *p1)
{
	Q_UNUSED(p1); return Text::null;
}

Value Text::getInParents(GET_METHOD_ARGS) const
{
    Object proto = TextProto::self(p1);
    if(proto.hasProperty(p1,p2)) return proto.get(p1,p2);
    if(CharacterData::hasProperty(p1,p2)) return CharacterData::get(p1,p2,p3);
    return Undefined();
}

bool Text::put(PUT_METHOD_ARGS)
{
    if(CharacterData::hasProperty(p1,p2)) {
        CharacterData::put(p1,p2,p3,p4);
        return true;
    }
    return false;
}

Object Text::prototype(ExecState *p1) const
{
    if(p1) return TextProto::self(p1);
    return Object::dynamicCast(Null());
}

ObjectImp *Text::bridge(ExecState *p1) const
{
    return new DOMRWBridge<Text>(p1,static_cast<Text::Private *>(d));
}

Value Text::cache(ExecState *p1) const
{
    return cacheDOMObject<Text>(p1,static_cast<Text::Private *>(d));
}

bool TreeWalker::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&TreeWalker::s_hashTable,p2);
    if(e) return true;
    Object proto = TreeWalkerProto::self(p1);
    if(proto.hasProperty(p1,p2)) return true;
    return false;
}

Value TreeWalker::get(GET_METHOD_ARGS) const
{
    return lookupGet<TreeWalkerProtoFunc,TreeWalker>(p1,p2,&s_hashTable,this,p3);
}

TreeWalker TreeWalkerProtoFunc::cast(KJS::ExecState *exec, const ObjectImp *p1) const
{
    { const DOMBridge<TreeWalker> *test = dynamic_cast<const DOMBridge<TreeWalker> * >(p1);
    if(test) return TreeWalker(static_cast<TreeWalker::Private *>(test->impl())); }
    if(!exec) return TreeWalker::null;
    ScriptInterpreter *interpreter = static_cast<ScriptInterpreter *>(exec->interpreter());
    DocumentImpl *document = (interpreter ? interpreter->document() : 0);
    Ecma *engine = (document ? document->ecmaEngine() : 0);
    EcmaInterface *interface = (engine ? engine->interface() : 0);
    if(!interface) return TreeWalker::null;
    return interface->inheritedTreeWalkerCast(p1);
}

TreeWalker KDOM::EcmaInterface::inheritedTreeWalkerCast(const ObjectImp *p1)
{
	Q_UNUSED(p1); return TreeWalker::null;
}

Value TreeWalker::getInParents(GET_METHOD_ARGS) const
{
    Object proto = TreeWalkerProto::self(p1);
    if(proto.hasProperty(p1,p2)) return proto.get(p1,p2);
    Q_UNUSED(p3);
    return Undefined();
}

bool TreeWalker::put(PUT_METHOD_ARGS)
{
    return lookupPut<TreeWalker>(p1,p2,p3,p4,&s_hashTable,this);
}

bool TreeWalker::putInParents(PUT_METHOD_ARGS)
{
    Q_UNUSED(p1); Q_UNUSED(p2); Q_UNUSED(p3); Q_UNUSED(p4);
    return false;
}

Object TreeWalker::prototype(ExecState *p1) const
{
    if(p1) return TreeWalkerProto::self(p1);
    return Object::dynamicCast(Null());
}

ObjectImp *TreeWalker::bridge(ExecState *p1) const
{
    return new DOMRWBridge<TreeWalker>(p1,static_cast<TreeWalker::Private *>(d));
}

Value TreeWalker::cache(ExecState *p1) const
{
    return cacheDOMObject<TreeWalker>(p1,static_cast<TreeWalker::Private *>(d));
}

bool TypeInfo::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&TypeInfo::s_hashTable,p2);
    if(e) return true;
    Object proto = TypeInfoProto::self(p1);
    if(proto.hasProperty(p1,p2)) return true;
    return false;
}

Value TypeInfo::get(GET_METHOD_ARGS) const
{
    return lookupGet<TypeInfoProtoFunc,TypeInfo>(p1,p2,&s_hashTable,this,p3);
}

TypeInfo TypeInfoProtoFunc::cast(KJS::ExecState *exec, const ObjectImp *p1) const
{
    { const DOMBridge<TypeInfo> *test = dynamic_cast<const DOMBridge<TypeInfo> * >(p1);
    if(test) return TypeInfo(static_cast<TypeInfo::Private *>(test->impl())); }
    if(!exec) return TypeInfo::null;
    ScriptInterpreter *interpreter = static_cast<ScriptInterpreter *>(exec->interpreter());
    DocumentImpl *document = (interpreter ? interpreter->document() : 0);
    Ecma *engine = (document ? document->ecmaEngine() : 0);
    EcmaInterface *interface = (engine ? engine->interface() : 0);
    if(!interface) return TypeInfo::null;
    return interface->inheritedTypeInfoCast(p1);
}

TypeInfo KDOM::EcmaInterface::inheritedTypeInfoCast(const ObjectImp *p1)
{
	Q_UNUSED(p1); return TypeInfo::null;
}

Value TypeInfo::getInParents(GET_METHOD_ARGS) const
{
    Object proto = TypeInfoProto::self(p1);
    if(proto.hasProperty(p1,p2)) return proto.get(p1,p2);
    Q_UNUSED(p3);
    return Undefined();
}

Object TypeInfo::prototype(ExecState *p1) const
{
    if(p1) return TypeInfoProto::self(p1);
    return Object::dynamicCast(Null());
}

ObjectImp *TypeInfo::bridge(ExecState *p1) const
{
    return new DOMBridge<TypeInfo>(p1,static_cast<TypeInfo::Private *>(d));
}

Value TypeInfo::cache(ExecState *p1) const
{
    return cacheDOMObject<TypeInfo>(p1,static_cast<TypeInfo::Private *>(d));
}

bool UIEvent::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&UIEvent::s_hashTable,p2);
    if(e) return true;
    Object proto = UIEventProto::self(p1);
    if(proto.hasProperty(p1,p2)) return true;
    if(Event::hasProperty(p1,p2)) return true;
    return false;
}

Value UIEvent::get(GET_METHOD_ARGS) const
{
    return lookupGet<UIEventProtoFunc,UIEvent>(p1,p2,&s_hashTable,this,p3);
}

UIEvent UIEventProtoFunc::cast(KJS::ExecState *exec, const ObjectImp *p1) const
{
    { const DOMBridge<UIEvent> *test = dynamic_cast<const DOMBridge<UIEvent> * >(p1);
    if(test) return UIEvent(static_cast<UIEvent::Private *>(test->impl())); }
    { const DOMBridge<KeyboardEvent> *test = dynamic_cast<const DOMBridge<KeyboardEvent> * >(p1);
    if(test) return UIEvent(static_cast<UIEvent::Private *>(test->impl())); }
    { const DOMBridge<MouseEvent> *test = dynamic_cast<const DOMBridge<MouseEvent> * >(p1);
    if(test) return UIEvent(static_cast<UIEvent::Private *>(test->impl())); }
    if(!exec) return UIEvent::null;
    ScriptInterpreter *interpreter = static_cast<ScriptInterpreter *>(exec->interpreter());
    DocumentImpl *document = (interpreter ? interpreter->document() : 0);
    Ecma *engine = (document ? document->ecmaEngine() : 0);
    EcmaInterface *interface = (engine ? engine->interface() : 0);
    if(!interface) return UIEvent::null;
    return interface->inheritedUIEventCast(p1);
}

UIEvent KDOM::EcmaInterface::inheritedUIEventCast(const ObjectImp *p1)
{
	Q_UNUSED(p1); return UIEvent::null;
}

Value UIEvent::getInParents(GET_METHOD_ARGS) const
{
    Object proto = UIEventProto::self(p1);
    if(proto.hasProperty(p1,p2)) return proto.get(p1,p2);
    if(Event::hasProperty(p1,p2)) return Event::get(p1,p2,p3);
    return Undefined();
}

Object UIEvent::prototype(ExecState *p1) const
{
    if(p1) return UIEventProto::self(p1);
    return Object::dynamicCast(Null());
}

ObjectImp *UIEvent::bridge(ExecState *p1) const
{
    return new DOMBridge<UIEvent>(p1,static_cast<UIEvent::Private *>(d));
}

Value UIEvent::cache(ExecState *p1) const
{
    return cacheDOMObject<UIEvent>(p1,static_cast<UIEvent::Private *>(d));
}

bool XPathEvaluator::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&XPathEvaluator::s_hashTable,p2);
    if(e) return true;
    Object proto = XPathEvaluatorProto::self(p1);
    if(proto.hasProperty(p1,p2)) return true;
    return false;
}

Value XPathEvaluator::get(GET_METHOD_ARGS) const
{
    return lookupGet<XPathEvaluatorProtoFunc,XPathEvaluator>(p1,p2,&s_hashTable,this,p3);
}

XPathEvaluator XPathEvaluatorProtoFunc::cast(KJS::ExecState *exec, const ObjectImp *p1) const
{
    { const DOMBridge<XPathEvaluator> *test = dynamic_cast<const DOMBridge<XPathEvaluator> * >(p1);
    if(test) return XPathEvaluator(static_cast<XPathEvaluator::Private *>(test->impl())); }
    { const DOMBridge<Document> *test = dynamic_cast<const DOMBridge<Document> * >(p1);
    if(test) return XPathEvaluator(static_cast<XPathEvaluator::Private *>(test->impl())); }
    if(!exec) return XPathEvaluator::null;
    ScriptInterpreter *interpreter = static_cast<ScriptInterpreter *>(exec->interpreter());
    DocumentImpl *document = (interpreter ? interpreter->document() : 0);
    Ecma *engine = (document ? document->ecmaEngine() : 0);
    EcmaInterface *interface = (engine ? engine->interface() : 0);
    if(!interface) return XPathEvaluator::null;
    return interface->inheritedXPathEvaluatorCast(p1);
}

XPathEvaluator KDOM::EcmaInterface::inheritedXPathEvaluatorCast(const ObjectImp *p1)
{
	Q_UNUSED(p1); return XPathEvaluator::null;
}

Value XPathEvaluator::getInParents(GET_METHOD_ARGS) const
{
    Object proto = XPathEvaluatorProto::self(p1);
    if(proto.hasProperty(p1,p2)) return proto.get(p1,p2);
    Q_UNUSED(p3);
    return Undefined();
}

Object XPathEvaluator::prototype(ExecState *p1) const
{
    if(p1) return XPathEvaluatorProto::self(p1);
    return Object::dynamicCast(Null());
}

ObjectImp *XPathEvaluator::bridge(ExecState *p1) const
{
    return new DOMBridge<XPathEvaluator>(p1,static_cast<XPathEvaluator::Private *>(d));
}

Value XPathEvaluator::cache(ExecState *p1) const
{
    return cacheDOMObject<XPathEvaluator>(p1,static_cast<XPathEvaluator::Private *>(d));
}

bool XPathException::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&XPathException::s_hashTable,p2);
    if(e) return true;
    Q_UNUSED(p1);
    return false;
}

Value XPathException::get(GET_METHOD_ARGS) const
{
    return lookupGetValue<XPathException>(p1,p2,&s_hashTable,this,p3);
}

Value XPathException::getInParents(GET_METHOD_ARGS) const
{
    Q_UNUSED(p1); Q_UNUSED(p2); Q_UNUSED(p3);
    return Undefined();
}

Object XPathException::prototype(ExecState *p1) const
{
    if(p1) return p1->interpreter()->builtinObjectPrototype();
    return Object::dynamicCast(Null());
}

ObjectImp *XPathException::bridge(ExecState *p1) const
{
    return new DOMBridge<XPathException>(p1,static_cast<XPathException::Private *>(d));
}

Value XPathException::cache(ExecState *p1) const
{
    return cacheDOMObject<XPathException>(p1,static_cast<XPathException::Private *>(d));
}

bool XPathExpression::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&XPathExpression::s_hashTable,p2);
    if(e) return true;
    Object proto = XPathExpressionProto::self(p1);
    if(proto.hasProperty(p1,p2)) return true;
    return false;
}

Value XPathExpression::get(GET_METHOD_ARGS) const
{
    return lookupGet<XPathExpressionProtoFunc,XPathExpression>(p1,p2,&s_hashTable,this,p3);
}

XPathExpression XPathExpressionProtoFunc::cast(KJS::ExecState *exec, const ObjectImp *p1) const
{
    { const DOMBridge<XPathExpression> *test = dynamic_cast<const DOMBridge<XPathExpression> * >(p1);
    if(test) return XPathExpression(static_cast<XPathExpression::Private *>(test->impl())); }
    if(!exec) return XPathExpression::null;
    ScriptInterpreter *interpreter = static_cast<ScriptInterpreter *>(exec->interpreter());
    DocumentImpl *document = (interpreter ? interpreter->document() : 0);
    Ecma *engine = (document ? document->ecmaEngine() : 0);
    EcmaInterface *interface = (engine ? engine->interface() : 0);
    if(!interface) return XPathExpression::null;
    return interface->inheritedXPathExpressionCast(p1);
}

XPathExpression KDOM::EcmaInterface::inheritedXPathExpressionCast(const ObjectImp *p1)
{
	Q_UNUSED(p1); return XPathExpression::null;
}

Value XPathExpression::getInParents(GET_METHOD_ARGS) const
{
    Object proto = XPathExpressionProto::self(p1);
    if(proto.hasProperty(p1,p2)) return proto.get(p1,p2);
    Q_UNUSED(p3);
    return Undefined();
}

Object XPathExpression::prototype(ExecState *p1) const
{
    if(p1) return XPathExpressionProto::self(p1);
    return Object::dynamicCast(Null());
}

ObjectImp *XPathExpression::bridge(ExecState *p1) const
{
    return new DOMBridge<XPathExpression>(p1,static_cast<XPathExpression::Private *>(d));
}

Value XPathExpression::cache(ExecState *p1) const
{
    return cacheDOMObject<XPathExpression>(p1,static_cast<XPathExpression::Private *>(d));
}

bool XPathNSResolver::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&XPathNSResolver::s_hashTable,p2);
    if(e) return true;
    Object proto = XPathNSResolverProto::self(p1);
    if(proto.hasProperty(p1,p2)) return true;
    return false;
}

Value XPathNSResolver::get(GET_METHOD_ARGS) const
{
    return lookupGet<XPathNSResolverProtoFunc,XPathNSResolver>(p1,p2,&s_hashTable,this,p3);
}

XPathNSResolver XPathNSResolverProtoFunc::cast(KJS::ExecState *exec, const ObjectImp *p1) const
{
    return KDOM::toXPathNSResolver(exec, p1);
}

XPathNSResolver KDOM::toXPathNSResolver(KJS::ExecState *exec, const ObjectImp *p1)
{
    { const DOMBridge<XPathNSResolver> *test = dynamic_cast<const DOMBridge<XPathNSResolver> * >(p1);
    if(test) return XPathNSResolver(static_cast<XPathNSResolver::Private *>(test->impl())); }
    if(!exec) return XPathNSResolver::null;
    ScriptInterpreter *interpreter = static_cast<ScriptInterpreter *>(exec->interpreter());
    DocumentImpl *document = (interpreter ? interpreter->document() : 0);
    Ecma *engine = (document ? document->ecmaEngine() : 0);
    EcmaInterface *interface = (engine ? engine->interface() : 0);
    if(!interface) return XPathNSResolver::null;
    return interface->inheritedXPathNSResolverCast(p1);
}

XPathNSResolver KDOM::EcmaInterface::inheritedXPathNSResolverCast(const ObjectImp *p1)
{
	Q_UNUSED(p1); return XPathNSResolver::null;
}

Value XPathNSResolver::getInParents(GET_METHOD_ARGS) const
{
    Object proto = XPathNSResolverProto::self(p1);
    if(proto.hasProperty(p1,p2)) return proto.get(p1,p2);
    Q_UNUSED(p3);
    return Undefined();
}

Object XPathNSResolver::prototype(ExecState *p1) const
{
    if(p1) return XPathNSResolverProto::self(p1);
    return Object::dynamicCast(Null());
}

ObjectImp *XPathNSResolver::bridge(ExecState *p1) const
{
    return new DOMBridge<XPathNSResolver>(p1,static_cast<XPathNSResolver::Private *>(d));
}

Value XPathNSResolver::cache(ExecState *p1) const
{
    return cacheDOMObject<XPathNSResolver>(p1,static_cast<XPathNSResolver::Private *>(d));
}

bool XPathNamespace::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&XPathNamespace::s_hashTable,p2);
    if(e) return true;
    if(Node::hasProperty(p1,p2)) return true;
    return false;
}

Value XPathNamespace::get(GET_METHOD_ARGS) const
{
    return lookupGetValue<XPathNamespace>(p1,p2,&s_hashTable,this,p3);
}

Value XPathNamespace::getInParents(GET_METHOD_ARGS) const
{
    if(Node::hasProperty(p1,p2)) return Node::get(p1,p2,p3);
    return Undefined();
}

bool XPathNamespace::put(PUT_METHOD_ARGS)
{
    if(Node::hasProperty(p1,p2)) {
        Node::put(p1,p2,p3,p4);
        return true;
    }
    return false;
}

Object XPathNamespace::prototype(ExecState *p1) const
{
    if(p1) return p1->interpreter()->builtinObjectPrototype();
    return Object::dynamicCast(Null());
}

ObjectImp *XPathNamespace::bridge(ExecState *p1) const
{
    return new DOMRWBridge<XPathNamespace>(p1,static_cast<XPathNamespace::Private *>(d));
}

Value XPathNamespace::cache(ExecState *p1) const
{
    return cacheDOMObject<XPathNamespace>(p1,static_cast<XPathNamespace::Private *>(d));
}

bool XPathResult::hasProperty(ExecState *p1,const Identifier &p2) const
{
    const HashEntry *e = Lookup::findEntry(&XPathResult::s_hashTable,p2);
    if(e) return true;
    Object proto = XPathResultProto::self(p1);
    if(proto.hasProperty(p1,p2)) return true;
    return false;
}

Value XPathResult::get(GET_METHOD_ARGS) const
{
    return lookupGet<XPathResultProtoFunc,XPathResult>(p1,p2,&s_hashTable,this,p3);
}

XPathResult XPathResultProtoFunc::cast(KJS::ExecState *exec, const ObjectImp *p1) const
{
    { const DOMBridge<XPathResult> *test = dynamic_cast<const DOMBridge<XPathResult> * >(p1);
    if(test) return XPathResult(static_cast<XPathResult::Private *>(test->impl())); }
    if(!exec) return XPathResult::null;
    ScriptInterpreter *interpreter = static_cast<ScriptInterpreter *>(exec->interpreter());
    DocumentImpl *document = (interpreter ? interpreter->document() : 0);
    Ecma *engine = (document ? document->ecmaEngine() : 0);
    EcmaInterface *interface = (engine ? engine->interface() : 0);
    if(!interface) return XPathResult::null;
    return interface->inheritedXPathResultCast(p1);
}

XPathResult KDOM::EcmaInterface::inheritedXPathResultCast(const ObjectImp *p1)
{
	Q_UNUSED(p1); return XPathResult::null;
}

Value XPathResult::getInParents(GET_METHOD_ARGS) const
{
    Object proto = XPathResultProto::self(p1);
    if(proto.hasProperty(p1,p2)) return proto.get(p1,p2);
    Q_UNUSED(p3);
    return Undefined();
}

Object XPathResult::prototype(ExecState *p1) const
{
    if(p1) return XPathResultProto::self(p1);
    return Object::dynamicCast(Null());
}

ObjectImp *XPathResult::bridge(ExecState *p1) const
{
    return new DOMBridge<XPathResult>(p1,static_cast<XPathResult::Private *>(d));
}

Value XPathResult::cache(ExecState *p1) const
{
    return cacheDOMObject<XPathResult>(p1,static_cast<XPathResult::Private *>(d));
}

