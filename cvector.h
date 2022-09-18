#ifndef CVECTOR_H
#define CVECTOR_H

// All methods return a success status, and set the error variable if they fail.

// Macro so you can provide just the datatype without having to use sizeof()
#define Vec(obj) Vec_create(sizeof(obj))

// data is just an array, so you can cast it and use it as such.
typedef struct {
	void *data;
	size_t len;
} Vec;

// void Vec_free_t(void *ptr);
typedef void (*Vec_free_t)(void*);

// void *Vec_copy_t(void *dest, void *src);
typedef void *(*Vec_copy_t)(void*, void*);

// Create a Vec with enough memory for initial_cap items
Vec *Vec_create_with_cap(size_t obj_size, size_t initial_cap);

// Takes the object size as a parameter
Vec *Vec_create(size_t obj_size);

// Create a Vec with the contents of an array. Supports deep copies.
Vec *Vec_create_from_array(void *src, Vec_copy_t copy_func, size_t obj_size, size_t len);

// Shortcut for stack arrays, since they usually have a stored length.
#define Vec_create_from_stack_array(src, copy_func) Vec_create_from_array(src, copy_func, sizeof(src[0]), sizeof(src) / sizeof(src[0]))

// Takes the object and a custom free function, or NULL if one is not required.
void Vec_destroy(Vec *this, Vec_free_t free_func);

/* Creates a copy of a vector. If no copy function is provided, the vector
 * data will be copied as if memcpy() was used to copy the data. */
Vec *Vec_copy(Vec *this, Vec_copy_t copy_func);

// Get an item from the desired index of a vector.
void *Vec_get(Vec *this, size_t index);

// Copy an item to the desired index of a vector.
int Vec_set(Vec *this, size_t index, void *new_obj);

/* Truncate or extend a vector to a given length. If the vector is extended,
 * the new data in the vector is uninitialized and the values are undefined.
 * However, you may initialize them yourself using memset. */
int Vec_resize(Vec *this, size_t len);

// Insert an item at the desired index of a vector.
int Vec_insert(Vec *this, void *new_obj, size_t index);

// Remove an item from the desired index of a vector
void *Vec_remove(Vec *this, size_t index);

// Push an item to the end of the vector.
int Vec_push(Vec *this, void *new_obj);

// Get the item at the top of the vector.
void *Vec_peek(Vec *this);

/* Pops the last item from the vector and removes it.
 * If you're going to use this, copy the result IMMEDIATELY.
 * This copies the popped item to an intermediate buffer,
 * which will be overwritten ever time an item is popped.
  * If this data needs to be freed, you must do it yourself! */
void *Vec_pop(Vec *this);

// Return the error status of the vector as a string.
const char *Vec_error(Vec *this);

#endif
