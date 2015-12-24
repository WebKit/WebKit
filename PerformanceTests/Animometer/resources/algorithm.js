function Heap(maxSize, compare)
{
    this._maxSize = maxSize;
    this._compare = compare;
    this._size = 0;
    this._values = new Array(this._maxSize);
}

Heap.prototype =
{
    // This is a binary heap represented in an array. The root element is stored
    // in the first element in the array. The root is followed by its two children.
    // Then its four grandchildren and so on. So every level in the binary heap is
    // doubled in the following level. Here is an example of the node indices and
    // how they are related to their parents and children.
    // ===========================================================================
    //              0       1       2       3       4       5       6
    // PARENT       -1      0       0       1       1       2       2
    // LEFT         1       3       5       7       9       11      13
    // RIGHT        2       4       6       8       10      12      14
    // ===========================================================================
    _parentIndex: function(i)
    {
        return i > 0 ? Math.floor((i - 1) / 2) : -1;
    },
    
    _leftIndex: function(i)
    {
        var leftIndex = i * 2 + 1;
        return leftIndex < this._size ? leftIndex : -1;
    },
    
    _rightIndex: function(i)
    {
        var rightIndex = i * 2 + 2;
        return rightIndex < this._size ? rightIndex : -1;
    },
    
    // Return the child index that may violate the heap property at index i.
    _childIndex: function(i)
    {
        var left = this._leftIndex(i);
        var right = this._rightIndex(i);

        if (left != -1 && right != -1)
            return this._compare(this._values[left], this._values[right]) > 0 ? left : right;
        
        return left != -1 ? left : right;
    },
    
    init: function()
    {
        this._size = 0;
    },
    
    top: function()
    {
        return this._size ? this._values[0] : NaN;
    },
    
    push: function(value)
    {
        if (this._size == this._maxSize) {
            // If size is bounded and the new value can be a parent of the top()
            // if the size were unbounded, just ignore the new value.
            if (this._compare(value, this.top()) > 0)
                return;
            this.pop();
        }
        this._values[this._size++] = value;
        this._bubble(this._size - 1);
    },

    pop: function()
    {
        if (!this._size)
            return NaN;
        
        this._values[0] = this._values[--this._size];
        this._sink(0);
    },
    
    _bubble: function(i)
    {
        // Fix the heap property at index i given that parent is the only node that
        // may violate the heap property.
        for (var pi = this._parentIndex(i); pi != -1; i = pi, pi = this._parentIndex(pi)) {
            if (this._compare(this._values[pi], this._values[i]) > 0)
                break;
                
            this._values.swap(pi, i);
        }
    },
    
    _sink: function(i)
    {
        // Fix the heap property at index i given that each of the left and the right
        // sub-trees satisfies the heap property.
        for (var ci = this._childIndex(i); ci != -1; i = ci, ci = this._childIndex(ci)) {
            if (this._compare(this._values[i], this._values[ci]) > 0)
                break;
            
            this._values.swap(ci, i);
        }
    },
    
    str: function()
    {
        var out = "Heap[" + this._size + "] = [";
        for (var i = 0; i < this._size; ++i) {
            out += this._values[i];
            if (i < this._size - 1)
                out += ", ";
        }
        return out + "]";
    },
    
    values: function(size) {
        // Return the last "size" heap elements values.
        var values = this._values.slice(0, this._size);
        return values.sort(this._compare).slice(0, Math.min(size, this._size));
    }
}

var Algorithm = {
    createMinHeap: function(maxSize)
    {
        return new Heap(maxSize, function(a, b) { return b - a; });
    },
    
    createMaxHeap: function(maxSize) {
        return new Heap(maxSize, function(a, b) { return a - b; });
    }
}
