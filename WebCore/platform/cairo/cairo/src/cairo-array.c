/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2004 Red Hat, Inc
 *
 * This library is free software; you can redistribute it and/or
 * modify it either under the terms of the GNU Lesser General Public
 * License version 2.1 as published by the Free Software Foundation
 * (the "LGPL") or, at your option, under the terms of the Mozilla
 * Public License Version 1.1 (the "MPL"). If you do not alter this
 * notice, a recipient may use your version of this file under either
 * the MPL or the LGPL.
 *
 * You should have received a copy of the LGPL along with this library
 * in the file COPYING-LGPL-2.1; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 * You should have received a copy of the MPL along with this library
 * in the file COPYING-MPL-1.1
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY
 * OF ANY KIND, either express or implied. See the LGPL or the MPL for
 * the specific language governing rights and limitations.
 *
 * The Original Code is the cairo graphics library.
 *
 * The Initial Developer of the Original Code is University of Southern
 * California.
 *
 * Contributor(s):
 *	Kristian Høgsberg <krh@redhat.com>
 *	Carl Worth <cworth@cworth.org>
 */

#include "cairoint.h"

/**
 * _cairo_array_init:
 * 
 * Initialize a new cairo_array object to store objects each of size
 * @element_size.
 *
 * The #cairo_array_t object provides grow-by-doubling storage. It
 * never intereprets the data passed to it, nor does it provide any
 * sort of callback mechanism for freeing resources held onto by
 * stored objects.
 *
 * When finished using the array, _cairo_array_fini() should be
 * called to free resources allocated during use of the array.
 **/
void
_cairo_array_init (cairo_array_t *array, int element_size)
{
    array->size = 0;
    array->num_elements = 0;
    array->element_size = element_size;
    array->elements = NULL;

    array->is_snapshot = FALSE;
}

/**
 * _cairo_array_init_snapshot:
 * @array: A #cairo_array_t to be initialized as a snapshot
 * @other: The #cairo_array_t from which to create the snapshot
 * 
 * Initialize @array as an immutable copy of @other. It is an error to
 * call an array-modifying function (other than _cairo_array_fini) on
 * @array after calling this function.
 **/
void
_cairo_array_init_snapshot (cairo_array_t	*array,
			    const cairo_array_t *other)
{
    array->size = other->size;
    array->num_elements = other->num_elements;
    array->element_size = other->element_size;
    array->elements = other->elements;

    array->is_snapshot = TRUE;
}

/**
 * _cairo_array_fini:
 *
 * Free all resources associated with @array. After this call, @array
 * should not be used again without a subsequent call to
 * _cairo_array_init() again first.
 **/
void
_cairo_array_fini (cairo_array_t *array)
{
    if (array->is_snapshot)
	return;

    if (array->elements) {
	free (* array->elements);
	free (array->elements);
    }
}

/**
 * _cairo_array_grow_by:
 * 
 * Increase the size of @array (if needed) so that there are at least
 * @additional free spaces in the array. The actual size of the array
 * is always increased by doubling as many times as necessary.
 **/
cairo_status_t
_cairo_array_grow_by (cairo_array_t *array, int additional)
{
    char *new_elements;
    int old_size = array->size;
    int required_size = array->num_elements + additional;
    int new_size;

    assert (! array->is_snapshot);

    if (required_size <= old_size)
	return CAIRO_STATUS_SUCCESS;

    if (old_size == 0)
	new_size = 1;
    else
	new_size = old_size * 2;

    while (new_size < required_size)
	new_size = new_size * 2;

    if (array->elements == NULL) {
	array->elements = malloc (sizeof (char *));
	if (array->elements == NULL)
	    return CAIRO_STATUS_NO_MEMORY;
	*array->elements = NULL;
    }

    array->size = new_size;
    new_elements = realloc (*array->elements,
			    array->size * array->element_size);

    if (new_elements == NULL) {
	array->size = old_size;
	return CAIRO_STATUS_NO_MEMORY;
    }

    *array->elements = new_elements;

    return CAIRO_STATUS_SUCCESS;
}

/**
 * _cairo_array_truncate:
 * 
 * Truncate size of the array to @num_elements if less than the
 * current size. No memory is actually freed. The stored objects
 * beyond @num_elements are simply "forgotten".
 **/
void
_cairo_array_truncate (cairo_array_t *array, int num_elements)
{
    assert (! array->is_snapshot);

    if (num_elements < array->num_elements)
	array->num_elements = num_elements;
}

/**
 * _cairo_array_index:
 * 
 * Return value: A pointer to object stored at @index. If the
 * resulting value is assigned to a pointer to an object of the same
 * element_size as initially passed to _cairo_array_init() then that
 * pointer may be used for further direct indexing with []. For
 * example:
 *
 * 	cairo_array_t array;
 *	double *values;
 *
 *	_cairo_array_init (&array, sizeof(double));
 *	... calls to _cairo_array_append() here ...
 *
 *	values = _cairo_array_index (&array, 0);
 *      for (i = 0; i < _cairo_array_num_elements (&array); i++)
 *	    ... use values[i] here ...
 **/
void *
_cairo_array_index (cairo_array_t *array, int index)
{
    /* We allow an index of 0 for the no-elements case.
     * This makes for cleaner calling code which will often look like:
     *
     *    elements = _cairo_array_index (array, num_elements);
     *	  for (i=0; i < num_elements; i++) {
     *        ... use elements[i] here ...
     *    }
     *
     * which in the num_elements==0 case gets the NULL pointer here,
     * but never dereferences it.
     */
    if (array->elements == NULL) {
	assert (index == 0);
	return NULL;
    }

    assert (0 <= index && index < array->num_elements);

    return (void *) &(*array->elements)[index * array->element_size];
}

/**
 * _cairo_array_copy_element:
 * 
 * Copy a single element out of the array from index @index into the
 * location pointed to by @dst.
 **/
void
_cairo_array_copy_element (cairo_array_t *array, int index, void *dst)
{
    memcpy (dst, _cairo_array_index (array, index), array->element_size);
}

/**
 * _cairo_array_append:
 * 
 * Append a single item onto the array by growing the array by at
 * least one element, then copying element_size bytes from @element
 * into the array. The address of the resulting object within the
 * array can be determined with:
 *
 * _cairo_array_index (array, _cairo_array_num_elements (array) - 1);
 *
 * Return value: CAIRO_STATUS_SUCCESS if successful or
 * CAIRO_STATUS_NO_MEMORY if insufficient memory is available for the
 * operation.
 **/
cairo_status_t
_cairo_array_append (cairo_array_t	*array,
		     const void		*element)
{
    assert (! array->is_snapshot);

    return _cairo_array_append_multiple (array, element, 1);
}

/**
 * _cairo_array_append:
 * 
 * Append one or more items onto the array by growing the array by
 * @num_elements, then copying @num_elements * element_size bytes from
 * @elements into the array.
 *
 * Return value: CAIRO_STATUS_SUCCESS if successful or
 * CAIRO_STATUS_NO_MEMORY if insufficient memory is available for the
 * operation.
 **/
cairo_status_t
_cairo_array_append_multiple (cairo_array_t	*array,
			      const void	*elements,
			      int		 num_elements)
{
    cairo_status_t status;
    void *dest;

    assert (! array->is_snapshot);

    status = _cairo_array_allocate (array, num_elements, &dest);
    if (status)
	return status;

    memcpy (dest, elements, num_elements * array->element_size);

    return CAIRO_STATUS_SUCCESS;
}

/**
 * _cairo_array_allocate:
 * 
 * Allocate space at the end of the array for @num_elements additional
 * elements, providing the address of the new memory chunk in
 * @elements. This memory will be unitialized, but will be accounted
 * for in the return value of _cairo_array_num_elements().
 * 
 * Return value: CAIRO_STATUS_SUCCESS if successful or
 * CAIRO_STATUS_NO_MEMORY if insufficient memory is available for the
 * operation.
 **/
cairo_status_t
_cairo_array_allocate (cairo_array_t	 *array,
		       int		  num_elements,
		       void		**elements)
{
    cairo_status_t status;

    assert (! array->is_snapshot);

    status = _cairo_array_grow_by (array, num_elements);
    if (status)
	return status;

    assert (array->num_elements + num_elements <= array->size);

    *elements = &(*array->elements)[array->num_elements * array->element_size];

    array->num_elements += num_elements;

    return CAIRO_STATUS_SUCCESS;
}

/**
 * _cairo_array_num_elements:
 * 
 * Return value: The number of elements stored in @array.
 **/
int
_cairo_array_num_elements (cairo_array_t *array)
{
    return array->num_elements;
}

/* cairo_user_data_array_t */

typedef struct {
    const cairo_user_data_key_t *key;
    void *user_data;
    cairo_destroy_func_t destroy;
} cairo_user_data_slot_t;

/**
 * _cairo_user_data_array_init:
 * @array: a #cairo_user_data_array_t
 * 
 * Initializes a #cairo_user_data_array_t structure for future
 * use. After initialization, the array has no keys. Call
 * _cairo_user_data_array_fini() to free any allocated memory
 * when done using the array.
 **/
void
_cairo_user_data_array_init (cairo_user_data_array_t *array)
{
    _cairo_array_init (array, sizeof (cairo_user_data_slot_t));
}

/**
 * _cairo_user_data_array_fini:
 * @array: a #cairo_user_data_array_t
 * 
 * Destroys all current keys in the user data array and deallocates
 * any memory allocated for the array itself.
 **/
void
_cairo_user_data_array_fini (cairo_user_data_array_t *array)
{
    int i, num_slots;
    cairo_user_data_slot_t *slots;

    num_slots = array->num_elements;
    slots = _cairo_array_index (array, 0);
    for (i = 0; i < num_slots; i++) {
	if (slots[i].user_data != NULL && slots[i].destroy != NULL)
	    slots[i].destroy (slots[i].user_data);
    }

    _cairo_array_fini (array);
}

/**
 * _cairo_user_data_array_get_data:
 * @array: a #cairo_user_data_array_t
 * @key: the address of the #cairo_user_data_key_t the user data was
 * attached to
 * 
 * Returns user data previously attached using the specified
 * key.  If no user data has been attached with the given key this
 * function returns %NULL.
 * 
 * Return value: the user data previously attached or %NULL.
 **/
void *
_cairo_user_data_array_get_data (cairo_user_data_array_t     *array,
				 const cairo_user_data_key_t *key)
{
    int i, num_slots;
    cairo_user_data_slot_t *slots;

    /* We allow this to support degenerate objects such as
     * cairo_image_surface_nil. */
    if (array == NULL)
	return NULL;

    num_slots = array->num_elements;
    slots = _cairo_array_index (array, 0);
    for (i = 0; i < num_slots; i++) {
	if (slots[i].key == key)
	    return slots[i].user_data;
    }

    return NULL;
}

/**
 * _cairo_user_data_array_set_data:
 * @array: a #cairo_user_data_array_t
 * @key: the address of a #cairo_user_data_key_t to attach the user data to
 * @user_data: the user data to attach
 * @destroy: a #cairo_destroy_func_t which will be called when the
 * user data array is destroyed or when new user data is attached using the
 * same key.
 * 
 * Attaches user data to a user data array.  To remove user data,
 * call this function with the key that was used to set it and %NULL
 * for @data.
 *
 * Return value: %CAIRO_STATUS_SUCCESS or %CAIRO_STATUS_NO_MEMORY if a
 * slot could not be allocated for the user data.
 **/
cairo_status_t
_cairo_user_data_array_set_data (cairo_user_data_array_t     *array,
				 const cairo_user_data_key_t *key,
				 void			     *user_data,
				 cairo_destroy_func_t	      destroy)
{
    cairo_status_t status;
    int i, num_slots;
    cairo_user_data_slot_t *slots, *slot, new_slot;

    if (user_data) {
	new_slot.key = key;
	new_slot.user_data = user_data;
	new_slot.destroy = destroy;
    } else {
	new_slot.key = NULL;
	new_slot.user_data = NULL;
	new_slot.destroy = NULL;
    }

    slot = NULL;
    num_slots = array->num_elements;
    slots = _cairo_array_index (array, 0);
    for (i = 0; i < num_slots; i++) {
	if (slots[i].key == key) {
	    slot = &slots[i];
	    if (slot->destroy && slot->user_data)
		slot->destroy (slot->user_data);
	    break;
	}
	if (user_data && slots[i].user_data == NULL) {
	    slot = &slots[i];	/* Have to keep searching for an exact match */
	}
    }

    if (slot) {
	*slot = new_slot;
	return CAIRO_STATUS_SUCCESS;
    }

    status = _cairo_array_append (array, &new_slot);
    if (status)
	return status;

    return CAIRO_STATUS_SUCCESS;
}

