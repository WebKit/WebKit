#ifndef DataGridColumnList_h
#define DataGridColumnList_h

#include "DataGridColumn.h"

#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>

namespace WebCore {

class AtomicString;

class DataGridColumnList : public RefCounted<DataGridColumnList> {
public:
    static PassRefPtr<DataGridColumnList> create()
    {
        return new DataGridColumnList();
    }
    
    ~DataGridColumnList();

    unsigned length() const { return m_columns.size(); }
    
    PassRefPtr<DataGridColumn> item(unsigned index) const { return m_columns[index]; }
    PassRefPtr<DataGridColumn> itemWithName(const AtomicString&) const;
    
    PassRefPtr<DataGridColumn> primaryColumn() const { return m_primaryColumn; }
    
    PassRefPtr<DataGridColumn> sortColumn() const { return m_sortColumn; }

    PassRefPtr<DataGridColumn> add(const String& id, const String& label, const String& type, bool primary, unsigned short sortable);
    void remove(DataGridColumn*);
    void move(DataGridColumn*, unsigned long index);
    void clear();
    
private:
    void primaryColumnChanged(DataGridColumn* col);

private:
    Vector<RefPtr<DataGridColumn> > m_columns;
    RefPtr<DataGridColumn> m_primaryColumn;
    RefPtr<DataGridColumn> m_sortColumn;
    
friend class DataGridColumn;
};

} // namespace WebCore

#endif // DataGridColumnList_h
