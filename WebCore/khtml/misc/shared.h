#ifndef SHARED_H
#define SHARED_H

#include <kxmlcore/RefPtr.h>

namespace khtml {

template<class type> class Shared
{
public:
    Shared() { _ref=0; /*counter++;*/ }
    ~Shared() { /*counter--;*/ }

    void ref() { _ref++;  }
    void deref() { 
	if(_ref) _ref--; 
	if(!_ref)
	    delete static_cast<type *>(this); 
    }
    bool hasOneRef() { //kdDebug(300) << "ref=" << _ref << endl;
    	return _ref==1; }

    int refCount() const { return _ref; }
//    static int counter;
protected:
    unsigned int _ref;

private:
    Shared(const Shared &);
    Shared &operator=(const Shared &);
};

template<class type> class TreeShared
{
public:
    TreeShared() { _ref = 0; m_parent = 0; /*counter++;*/ }
    TreeShared( type *parent ) { _ref=0; m_parent = parent; /*counter++;*/ }
    virtual ~TreeShared() { /*counter--;*/ }

    virtual void removedLastRef() { delete static_cast<type *>(this); }
    void ref() { _ref++;  }
    void deref() { 
	if(_ref) _ref--; 
	if(!_ref && !m_parent)
	    removedLastRef();
    }
    bool hasOneRef() { //kdDebug(300) << "ref=" << _ref << endl;
    	return _ref==1; }

    int refCount() const { return _ref; }
//    static int counter;

    void setParent(type *parent) { m_parent = parent; }
    type *parent() const { return m_parent; }
private:
    unsigned int _ref;
protected:
    type *m_parent;

private:
    TreeShared(const TreeShared &);
    TreeShared &operator=(const TreeShared &);
};

}

#endif
