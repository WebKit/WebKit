#ifndef SHARED_H
#define SHARED_H

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
};

template<class type> class TreeShared
{
public:
    TreeShared() { _ref = 0; m_parent = 0; /*counter++;*/ }
    TreeShared( type *parent ) { _ref=0; m_parent = parent; /*counter++;*/ }
    ~TreeShared() { /*counter--;*/ }

    void ref() { _ref++;  }
    void deref() { 
	if(_ref) _ref--; 
	if(!_ref && !m_parent) {
	    delete static_cast<type *>(this); 
	}
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
};

template <class T> class SharedPtr
{
public:
    SharedPtr() : m_ptr(0) {}
	explicit SharedPtr(T *ptr) : m_ptr(ptr) { if (m_ptr) m_ptr->ref(); }
	SharedPtr(const SharedPtr &o) : m_ptr(o.m_ptr) { if (m_ptr) m_ptr->ref(); }
    ~SharedPtr() { if (m_ptr) m_ptr->deref(); }
	
    bool isNull() const { return m_ptr == 0; }
    bool notNull() const { return m_ptr != 0; }

    void reset() { if (m_ptr) m_ptr->deref(); m_ptr = 0; }
    
    T * get() const { return m_ptr; }
	T &operator*() const { return *m_ptr; }
	T *operator->() const { return m_ptr; }

	bool operator!() const { return m_ptr == 0; }

	inline friend bool operator==(const SharedPtr &a, const SharedPtr &b) { return a.m_ptr == b.m_ptr; }
	inline friend bool operator==(const SharedPtr &a, const T *b) { return a.m_ptr == b; }
	inline friend bool operator==(const T *a, const SharedPtr &b) { return a == b.m_ptr; }

	SharedPtr &operator=(const SharedPtr &);

private:
	T* m_ptr;
};

template <class T> SharedPtr<T> &SharedPtr<T>::operator=(const SharedPtr<T> &o) 
{
	if (m_ptr != o.m_ptr) {
		if (m_ptr)
            m_ptr->deref();
		m_ptr = o.m_ptr;
		if (m_ptr) 
            m_ptr->ref();
	}
	
	return *this;
}

template <class T> inline bool operator!=(const SharedPtr<T> &a, const SharedPtr<T> &b) { return !(a==b); }
template <class T> inline bool operator!=(const SharedPtr<T> &a, const T *b) { return !(a == b); }
template <class T> inline bool operator!=(const T *a, const SharedPtr<T> &b) { return !(a == b); }

};

#endif
