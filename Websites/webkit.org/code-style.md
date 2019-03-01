### Indentation

[](#indentation-no-tabs) Use spaces, not tabs. Tabs should only appear in files that require them for semantic meaning, like Makefiles.

[](#indentation-4-spaces) The indent size is 4 spaces.

###### Right:

```cpp
int main()
{
    return 0;
}
```

###### Wrong:

```cpp
int main() 
{
        return 0;
}
```

[](#indentation-namespace) The contents of an outermost `namespace` block (and any nested namespaces with the same scope) should not be indented. The contents of other nested namespaces should be indented.

###### Right:

```cpp
// Document.h
namespace WebCore {

class Document {
    Document();
    ...
};

namespace NestedNamespace {
    ...
}

} // namespace WebCore

// Document.cpp
namespace WebCore {

Document::Document()
{
    ...
}

} // namespace WebCore
```

###### Wrong:

```cpp
// Document.h
namespace WebCore {

    class Document {
        Document();
        ...
    };

    namespace NestedNamespace {
    ...
    }

} // namespace WebCore

// Document.cpp
namespace WebCore {

    Document::Document()
    {
        ...
    }

} // namespace WebCore
```

[](#indentation-case-label) A case label should line up with its switch statement. The case statement is indented.

###### Right:

```cpp
switch (condition) {
case fooCondition:
case barCondition:
    i++;
    break;
default:
    i--;
}
```

###### Wrong:

```cpp
switch (condition) {
    case fooCondition:
    case barCondition:
        i++;
        break;
    default:
        i--;
}
```

[](#indentation-wrap-bool-op) Boolean expressions at the same nesting level that span multiple lines should have their operators on the left side of the line instead of the right side.

###### Right:

```cpp
return attribute.name() == srcAttr
    || attribute.name() == lowsrcAttr
    || (attribute.name() == usemapAttr && attribute.value().string()[0] != '#');
```


###### Wrong:

```cpp
return attribute.name() == srcAttr ||
    attribute.name() == lowsrcAttr ||
    (attribute.name() == usemapAttr && attr->value().string()[0] != '#');
```


### Spacing

[](#spacing-unary-op) Do not place spaces around unary operators.

###### Right:

```cpp
i++;
```


###### Wrong:

```cpp
i ++;
```

[](#spacing-binary-ternary-op) **Do** place spaces around binary and ternary operators.

###### Right:

```cpp
y = m * x + b;
f(a, b);
c = a | b;
return condition ? 1 : 0;
```

###### Wrong:

```cpp
y=m*x+b;
f(a,b);
c = a|b;
return condition ? 1:0;
```

[](#spacing-for-colon) Place spaces around the colon in a range-based for loop.

###### Right:

```cpp
Vector<PluginModuleInfo> plugins;
for (auto& plugin : plugins)
    registerPlugin(plugin);
```

###### Wrong:

```cpp
Vector<PluginModuleInfo> plugins;
for (auto& plugin: plugins)
    registerPlugin(plugin);
```

[](#spacing-comma-semicolon) Do not place spaces before comma and semicolon.

###### Right:

```cpp
for (int i = 0; i < 10; ++i)
    doSomething();

f(a, b);
```

###### Wrong:

```cpp
for (int i = 0 ; i < 10 ; ++i)
    doSomething();

f(a , b) ;
```

[](#spacing-control-paren) Place spaces between control statements and their parentheses.

###### Right:

```cpp
if (condition)
    doIt();
```

###### Wrong:

```cpp
if(condition)
    doIt();
```

[](#spacing-function-paren) Do not place spaces between a function and its parentheses, or between a parenthesis and its content.

###### Right:

```cpp
f(a, b);
```

###### Wrong:

```cpp
f (a, b);
f( a, b );
```

[](#spacing-braced-init) When initializing an object, place a space before the leading brace as well as between the braces and their content.

###### Right:

```cpp
Foo foo { bar };
```

###### Wrong:

```cpp
Foo foo{ bar };
Foo foo {bar};
```

[](#spacing-objc-block) In Objective-C, do not place spaces between the start of a block and its arguments, or the start of a block and its opening brace. **Do** place a space between argument lists and the opening brace of the block.

###### Right:

```cpp
block = ^{
...
};

block = ^(int, int) {
...
};

```

###### Wrong:

```cpp
block = ^ {
...
};

block = ^ (int, int){
...
};

```

### Line breaking

[](#linebreaking-multiple-statements) Each statement should get its own line.

###### Right:

```cpp
x++;
y++;
if (condition)
    doIt();
```

###### Wrong:

```cpp
x++; y++;
if (condition) doIt();
```

[](#linebreaking-else-braces) An `else` statement should go on the same line as a preceding close brace if one is present, else it should line up with the `if` statement.

###### Right:

```cpp
if (condition) {
    ...
} else {
    ...
}

if (condition)
    doSomething();
else
    doSomethingElse();

if (condition)
    doSomething();
else {
    ...
}
```

###### Wrong:

```cpp
if (condition) {
    ...
}
else {
    ...
}

if (condition) doSomething(); else doSomethingElse();

if (condition) doSomething(); else {
    ...
}
```

[](#linebreaking-else-if) An `else if` statement should be written as an `if` statement when the prior `if` concludes with a `return` statement.

###### Right:

```cpp
if (condition) {
    ...
    return someValue;
}
if (condition) {
    ...
}
```

###### Wrong:

```cpp
if (condition) {
    ...
    return someValue;
} else if (condition) {
    ...
}
```

### Braces

[](#braces-function) Function definitions: place each brace on its own line.

###### Right:

```cpp
int main()
{
    ...
}
```

###### Wrong:

```cpp
int main() {
    ...
}
```

[](#braces-blocks) Other braces: place the open brace on the line preceding the code block; place the close brace on its own line.

###### Right:

```cpp
class MyClass {
    ...
};

namespace WebCore {
    ...
}

for (int i = 0; i < 10; ++i) {
    ...
}
```

###### Wrong:

```cpp
class MyClass 
{
    ...
};
```

[](#braces-one-line) One-line control clauses should not use braces unless comments are included or a single statement spans multiple lines.

###### Right:

```cpp
if (condition)
    doIt();

if (condition) {
    // Some comment
    doIt();
}

if (condition) {
    myFunction(reallyLongParam1, reallyLongParam2, ...
        reallyLongParam5);
}
```

###### Wrong:

```cpp
if (condition) {
    doIt();
}

if (condition)
    // Some comment
    doIt();

if (condition)
    myFunction(reallyLongParam1, reallyLongParam2, ...
        reallyLongParam5);
```

[](#braces-empty-block) Control clauses without a body should use empty braces:

###### Right:

```cpp
for ( ; current; current = current->next) { }
```

###### Wrong:

```cpp
for ( ; current; current = current->next);
```

### Null, false and zero

[](#zero-null) In C++, the null pointer value should be written as `nullptr`. In C, it should be written as `NULL`. In Objective-C and Objective-C++, follow the guideline for C or C++, respectively, but use `nil` to represent a null Objective-C object.

[](#zero-bool) C++ and C `bool` values should be written as `true` and `false`. Objective-C `BOOL` values should be written as `YES` and `NO`.

[](#zero-comparison) Tests for true/false, null/non-null, and zero/non-zero should all be done without equality comparisons.

###### Right:

```cpp
if (condition)
    doIt();

if (!ptr)
    return;

if (!count)
    return;
```

###### Wrong:

```cpp
if (condition == true)
    doIt();

if (ptr == NULL)
    return;

if (count == 0)
    return;
```

[](#zero-objc-variables) In Objective-C, instance variables are initialized to zero automatically. Don't add explicit initializations to nil or NO in an init method.

### Floating point literals

[](#float-suffixes) Unless required in order to force floating point math, do not append `.0`, `.f` and `.0f` to floating point literals.

###### Right:

```cpp
const double duration = 60;

void setDiameter(float diameter)
{
    radius = diameter / 2;
}

setDiameter(10);

const int framesPerSecond = 12;
double frameDuration = 1.0 / framesPerSecond;
```

###### Wrong:

```cpp
const double duration = 60.0;

void setDiameter(float diameter)
{
    radius = diameter / 2.f;
}

setDiameter(10.f);

const int framesPerSecond = 12;
double frameDuration = 1 / framesPerSecond; // integer division
```

### Names

[](#names-basic) Use CamelCase. Capitalize the first letter, including all letters in an acronym, in a class, struct, protocol, or namespace name. Lower-case the first letter, including all letters in an acronym, in a variable or function name.

###### Right:

```cpp
struct Data;
size_t bufferSize;
class HTMLDocument;
String mimeType();
```

###### Wrong:

```cpp
struct data;
size_t buffer_size;
class HtmlDocument;
String MIMEType();
```

[](#names-full-words) Use full words, except in the rare case where an abbreviation would be more canonical and easier to understand.

###### Right:

```cpp
size_t characterSize;
size_t length;
short tabIndex; // more canonical
```

###### Wrong:

```cpp
size_t charSize;
size_t len;
short tabulationIndex; // bizarre
```

[](#names-data-members) Data members in C++ classes should be private. Static data members should be prefixed by "s_". Other data members should be prefixed by "m_".

###### Right:

```cpp
class String {
public:
    ...

private:
    short m_length;
};
```

###### Wrong:

```cpp
class String {
public:
    ...

    short length;
};
```

[](#names-objc-instance-variables) Prefix Objective-C instance variables with "_".

###### Right:

```cpp
@class String
    ...
    short _length;
@end
```

###### Wrong:

```cpp
@class String
    ...
    short length;
@end
```

[](#names-bool) Precede boolean values with words like "is" and "did".

###### Right:

```cpp
bool isValid;
bool didSendData;
```

###### Wrong:

```cpp
bool valid;
bool sentData;
```

[](#names-setter-getter) Precede setters with the word "set". Use bare words for getters. Setter and getter names should match the names of the variables being set/gotten.

###### Right:

```cpp
void setCount(size_t); // sets m_count
size_t count(); // returns m_count
```

###### Wrong:

```cpp
void setCount(size_t); // sets m_theCount
size_t getCount();
```

[](#names-out-argument) Precede getters that return values through out arguments with the word "get".

###### Right:

```cpp
void getInlineBoxAndOffset(InlineBox*&, int& caretOffset) const;
```

###### Wrong:

```cpp
void inlineBoxAndOffset(InlineBox*&, int& caretOffset) const;
```

[](#names-verb) Use descriptive verbs in function names.

###### Right:

```cpp
bool convertToASCII(short*, size_t);
```

###### Wrong:

```cpp
bool toASCII(short*, size_t);
```

[](#names-if-exists) The getter function for a member variable should not have any suffix or prefix indicating the function can optionally create or initialize the member variable. Suffix the getter function which does not automatically create the object with `IfExists` if there is a variant which does.

###### Right:

```cpp
StyleResolver* styleResolverIfExists();
StyleResolver& styleResolver();
```

###### Wrong:

```cpp
StyleResolver* styleResolver();
StyleResolver& ensureStyleResolver();
```

###### Right:

```cpp
Frame* frame();
```

###### Wrong:

```cpp
Frame* frameIfExists();
```

[](#names-variable-name-in-function-decl) Leave meaningless variable names out of function declarations. A good rule of thumb is if the parameter type name contains the parameter name (without trailing numbers or pluralization), then the parameter name isn't needed. Usually, there should be a parameter name for bools, strings, and numerical types.

###### Right:

```cpp
void setCount(size_t);

void doSomething(ScriptExecutionContext*);
```

###### Wrong:

```cpp
void setCount(size_t count);

void doSomething(ScriptExecutionContext* context);
```

[](#names-enum-to-bool) Prefer enums to bools on function parameters if callers are likely to be passing constants, since named constants are easier to read at the call site. An exception to this rule is a setter function, where the name of the function already makes clear what the boolean is.

###### Right:

```cpp
doSomething(something, AllowFooBar);
paintTextWithShadows(context, ..., textStrokeWidth > 0, isHorizontal());
setResizable(false);
```

###### Wrong:

```cpp
doSomething(something, false);
setResizable(NotResizable);
```

[](#names-objc-methods) Objective-C method names should follow the Cocoa naming guidelines — they should read like a phrase and each piece of the selector should start with a lowercase letter and use intercaps.

[](#names-enum-members) Enum members should use InterCaps with an initial capital letter.

[](#names-const-to-define) Prefer `const` to `#define`. Prefer inline functions to macros.

[](#names-define-constants) `#defined` constants should use all uppercase names with words separated by underscores.

[](#names-define-non-const) Macros that expand to function calls or other non-constant computation: these should be named like functions, and should have parentheses at the end, even if they take no arguments (with the exception of some special macros like ASSERT). Note that usually it is preferable to use an inline function in such cases instead of a macro.

###### Right:

```cpp
#define WBStopButtonTitle() \
        NSLocalizedString(@"Stop", @"Stop button title")
```

###### Wrong:

```cpp
#define WB_STOP_BUTTON_TITLE \
        NSLocalizedString(@"Stop", @"Stop button title")

#define WBStopButtontitle \
        NSLocalizedString(@"Stop", @"Stop button title")
```

[](#header-guards) Use `#pragma once` instead of `#define` and `#ifdef` for header guards.

###### Right:

```cpp
// HTMLDocument.h
#pragma once
```

###### Wrong:

```cpp
// HTMLDocument.h
#ifndef HTMLDocument_h
#define HTMLDocument_h
```

[](#names-protectors-this) Ref and RefPtr objects meant to protect `this` from deletion should be named "protectedThis".

###### Right:

```cpp
RefPtr<Node> protectedThis(this);
Ref<Element> protectedThis(*this);
RefPtr<Widget> protectedThis = this;
```

###### Wrong:

```cpp
RefPtr<Node> protector(this);
Ref<Node> protector = *this;
RefPtr<Widget> self(this);
Ref<Element> elementRef(*this);
```

[](#names-protectors) Ref and RefPtr objects meant to protect variables other than `this` from deletion should be named either "protector", or "protected" combined with the capitalized form of the variable name.

###### Right:

```cpp
RefPtr<Element> protector(&element);
RefPtr<Element> protector = &element;
RefPtr<Node> protectedNode(node);
RefPtr<Widget> protectedMainWidget(m_mainWidget);
RefPtr<Loader> protectedFontLoader = m_fontLoader;
```

###### Wrong:

```cpp
RefPtr<Node> nodeRef(&rootNode);
Ref<Element> protect(*element);
RefPtr<Node> protectorNode(node);
RefPtr<Widget> protected = widget;
```

### Other Punctuation

[](#punctuation-member-init) Constructors for C++ classes should initialize all of their members using C++ initializer syntax. Each member (and superclass) should be indented on a separate line, with the colon or comma preceding the member on that line.

###### Right:

```cpp
MyClass::MyClass(Document* document)
    : MySuperClass()
    , m_myMember(0)
    , m_document(document)
{
}

MyOtherClass::MyOtherClass()
    : MySuperClass()
{
}
```

###### Wrong:

```cpp
MyClass::MyClass(Document* document) : MySuperClass()
{
    m_myMember = 0;
    m_document = document;
}

MyOtherClass::MyOtherClass() : MySuperClass() {}
```

[](#punctuation-vector-index) Prefer index over iterator in Vector iterations for terse, easier-to-read code.

###### Right:

```cpp
for (auto& frameView : frameViews)
    frameView->updateLayoutAndStyleIfNeededRecursive();
```


#### OK:

```cpp
unsigned frameViewsCount = frameViews.size();
for (unsigned i = 0; i < frameViewsCount; ++i)
    frameViews[i]->updateLayoutAndStyleIfNeededRecursive();
```

###### Wrong:

```cpp
const Vector<RefPtr<FrameView> >::iterator end = frameViews.end();
for (Vector<RefPtr<FrameView> >::iterator it = frameViews.begin(); it != end; ++it)
    (*it)->updateLayoutAndStyleIfNeededRecursive();
```

### Pointers and References

[](#pointers-non-cpp) **Pointer types in non-C++ code**
Pointer types should be written with a space between the type and the `*` (so the `*` is adjacent to the following identifier if any).

[](#pointers-cpp) **Pointer and reference types in C++ code**
Both pointer types and reference types should be written with no space between the type name and the `*` or `&`.

###### Right:

```cpp
Image* SVGStyledElement::doSomething(PaintInfo& paintInfo)
{
    SVGStyledElement* element = static_cast<SVGStyledElement*>(node());
    const KCDashArray& dashes = dashArray();
```

###### Wrong:

```cpp
Image *SVGStyledElement::doSomething(PaintInfo &paintInfo)
{
    SVGStyledElement *element = static_cast<SVGStyledElement *>(node());
    const KCDashArray &dashes = dashArray();
```

[](#pointers-out-argument) An out argument of a function should be passed by reference except rare cases where it is optional in which case it should be passed by pointer.

###### Right:

```cpp
void MyClass::getSomeValue(OutArgumentType& outArgument) const
{
    outArgument = m_value;
}

void MyClass::doSomething(OutArgumentType* outArgument) const
{
    doSomething();
    if (outArgument)
        *outArgument = m_value;
}
```

###### Wrong:

```cpp
void MyClass::getSomeValue(OutArgumentType* outArgument) const
{
    *outArgument = m_value;
}
```

### #include Statements

[](#include-config-h) All implementation files must `#include` `config.h` first. Header files should never include `config.h`.

###### Right:

```cpp
// RenderLayer.h
#include "Node.h"
#include "RenderObject.h"
#include "RenderView.h"
```

###### Wrong:

```cpp
// RenderLayer.h
#include "config.h"

#include "RenderObject.h"
#include "RenderView.h"
#include "Node.h"
```

[](#include-primary) All implementation files must `#include` the primary header second, just after `config.h`. So for example, `Node.cpp` should include `Node.h` first, before other files. This guarantees that each header's completeness is tested. This also assures that each header can be compiled without requiring any other header files be included first.

[](#include-others) Other `#include` statements should be in sorted order (case sensitive, as done by the command-line sort tool or the Xcode sort selection command). Don't bother to organize them in a logical order.

###### Right:

```cpp
// HTMLDivElement.cpp
#include "config.h"
#include "HTMLDivElement.h"

#include "Attribute.h"
#include "HTMLElement.h"
#include "QualifiedName.h"
```

###### Wrong:

```cpp
// HTMLDivElement.cpp
#include "HTMLElement.h"
#include "HTMLDivElement.h"
#include "QualifiedName.h"
#include "Attribute.h"
```

[](#include-system) Includes of system headers must come after includes of other headers.

###### Right:

```cpp
// ConnectionQt.cpp
#include "ArgumentEncoder.h"
#include "ProcessLauncher.h"
#include "WebPageProxyMessageKinds.h"
#include "WorkItem.h"
#include <QApplication>
#include <QLocalServer>
#include <QLocalSocket>
```

###### Wrong:

```cpp
// ConnectionQt.cpp
#include "ArgumentEncoder.h"
#include "ProcessLauncher.h"
#include <QApplication>
#include <QLocalServer>
#include <QLocalSocket>
#include "WebPageProxyMessageKinds.h"
#include "WorkItem.h"
```

### "using" Statements

[](#using-in-headers) In header files, do not use "using" statements in namespace (or global) scope.

###### Right:

```cpp
// wtf/Vector.h

namespace WTF {

class VectorBuffer {
    using std::min;
    ...
};

} // namespace WTF
```

###### Wrong:

```cpp
// wtf/Vector.h

namespace WTF {

using std::min;

class VectorBuffer {
    ...
};

} // namespace WTF
```

[](#using-wtf) In header files in the WTF sub-library, however, it is acceptable to use "using" declarations at the end of the file to import one or more names in the WTF namespace into the global scope.

###### Right:

```cpp
// wtf/Vector.h

namespace WTF {

} // namespace WTF

using WTF::Vector;
```

###### Wrong:

```cpp
// wtf/Vector.h

namespace WTF {

} // namespace WTF

using namespace WTF;
```

###### Wrong:

```cpp
// runtime/JSObject.h

namespace WTF {

} // namespace WTF

using WTF::PlacementNewAdopt;
```

[](#using-in-cpp) In C++ implementation files, do not use "using" declarations of any kind to import names in the standard template library. Directly qualify the names at the point they're used instead.

###### Right:

```cpp
// HTMLBaseElement.cpp

namespace WebCore {

  std::swap(a, b);
  c = std::numeric_limits<int>::max()

} // namespace WebCore
```

###### Wrong:

```cpp
// HTMLBaseElement.cpp

using std::swap;

namespace WebCore {

  swap(a, b);

} // namespace WebCore
```

###### Wrong:

```cpp
// HTMLBaseElement.cpp

using namespace std;

namespace WebCore {

  swap(a, b);

} // namespace WebCore
```

[](#using-nested-namespaces) In implementation files, if a "using namespace" statement is for a nested namespace whose parent namespace is defined in the file, put the statement inside that namespace definition.

###### Right:

```cpp
// HTMLBaseElement.cpp

namespace WebCore {

using namespace HTMLNames;

} // namespace WebCore
```

###### Wrong:

```cpp
// HTMLBaseElement.cpp

using namespace WebCore::HTMLNames;

namespace WebCore {

} // namespace WebCore
```

[](#using-position) In implementation files, put all "using namespace" statements inside namespace definitions.

###### Right:

```cpp
// HTMLSelectElement.cpp

namespace WebCore {

using namespace other;

} // namespace WebCore
```

###### Wrong:

```cpp
// HTMLSelectElement.cpp

using namespace other;

namespace WebCore {

} // namespace WebCore
```

### Types

[](#types-unsigned) Omit "int" when using "unsigned" modifier. Do not use "signed" modifier. Use "int" by itself instead.

###### Right:

```cpp
unsigned a;
int b;
```

###### Wrong:

```cpp
unsigned int a; // Doesn't omit "int".
signed b; // Uses "signed" instead of "int".
signed int c; // Doesn't omit "signed".
```

### Classes

[](#classes-explicit) Use a constructor to do an implicit conversion when the argument is reasonably thought of as a type conversion and the type conversion is fast. Otherwise, use the explicit keyword or a function returning the type. This only applies to single argument constructors.

###### Right:

```cpp
class LargeInt {
public:
    LargeInt(int);
...

class Vector {
public:
    explicit Vector(int size); // Not a type conversion.
    Vector create(Array); // Costly conversion.
...

```

###### Wrong:

```cpp
class Task {
public:
    Task(ScriptExecutionContext&); // Not a type conversion.
    explicit Task(); // No arguments.
    explicit Task(ScriptExecutionContext&, Other); // More than one argument.
...
```

### Singleton pattern

[](#singleton-static-member) Use a static member function named "singleton()" to access the instance of the singleton.

###### Right:

```cpp
class MySingleton {
public:
    static MySingleton& singleton();
...
```

###### Wrong:

```cpp
class MySingleton {
public:
    static MySingleton& shared();
...
```

###### Wrong:

```cpp
class MySingleton {
...
};

MySingleton& mySingleton(); // free function.
```

### Comments

[](#comments-eol) Use only _one_ space before end of line comments and in between sentences in comments.

###### Right:

```cpp
f(a, b); // This explains why the function call was done. This is another sentence.
```

###### Wrong:

```cpp
int i;    // This is a comment with several spaces before it, which is a non-conforming style.
double f; // This is another comment.  There are two spaces before this sentence which is a non-conforming style.
```

[](#comments-sentences) Make comments look like sentences by starting with a capital letter and ending with a period (punctation). One exception may be end of line comments like this `if (x == y) // false for NaN`.

[](#comments-fixme) Use FIXME: (without attribution) to denote items that need to be addressed in the future.

###### Right:

```cpp
drawJpg(); // FIXME: Make this code handle jpg in addition to the png support.
```

###### Wrong:

```cpp
drawJpg(); // FIXME(joe): Make this code handle jpg in addition to the png support.
```

```cpp
drawJpg(); // TODO: Make this code handle jpg in addition to the png support.
```

### Overriding Virtual Methods

[](#override-methods) The base level declaration of a virtual method inside a class must be declared with the `virtual` keyword. All subclasses of that class must either specify the `override` keyword when overriding the virtual method or the `final` keyword when overriding the virtual method and requiring that no further subclasses can override it. You never want to annotate a method with more than one of the `virtual`, `override`, or `final` keywords.

###### Right:

```cpp
class Person {
public:
    virtual String description() { ... };
}

class Student : public Person {
public:
    String description() override { ... }; // This is correct because it only contains the "override" keyword to indicate that the method is overridden.
}

```

```cpp
class Person {
public:
    virtual String description() { ... };
}

class Student : public Person {
public:
    String description() final { ... }; // This is correct because it only contains the "final" keyword to indicate that the method is overridden and that no subclasses of "Student" can override "description".
}

```

###### Wrong:

```cpp
class Person {
public:
    virtual String description() { ... };
}

class Student : public Person {
public:
    virtual String description() override { ... }; // This is incorrect because it uses both the "virtual" and "override" keywords to indicate that the method is overridden. Instead, it should only use the "override" keyword.
}
```

```cpp
class Person {
public:
    virtual String description() { ... };
}

class Student : public Person {
public:
    virtual String description() final { ... }; // This is incorrect because it uses both the "virtual" and "final" keywords to indicate that the method is overridden and final. Instead, it should only use the "final" keyword.
}
```

```cpp
class Person {
public:
    virtual String description() { ... };
}

class Student : public Person {
public:
    virtual String description() { ... }; // This is incorrect because it uses the "virtual" keyword to indicate that the method is overridden.
}
```

### Python

[](#python) For Python use PEP8 style.
