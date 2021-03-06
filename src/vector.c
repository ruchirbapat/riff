#include "vector.h"
#include "generic.h"
#include "error.h"

#include <assert.h>
#include <string.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>

void vector_call_deletera(vector_t* v);
void vector_call_deletere(vector_t* v, size_t e);
void vector_call_deleteri(vector_t* v, size_t l, size_t r);

bool vector_realloc(vector_t* v, size_t len) {
	char* r = realloc(v->data, len * v->blksz);

	if (!r)
		return error_code(ERROR_ALLOCATION), false;

	v->data = r;
	v->capacity = len;

	return true;
}

void vector_call_deletere(vector_t* v, size_t e) {
	(*v->deleter)(*(void**)(v->data + (e * v->blksz)));
}

void vector_call_deletera(vector_t* v) {
	for (vector_iterator(v, void, b))
		(*v->deleter)(*(void**)b);
}

void vector_call_deleteri(vector_t* v, size_t l, size_t r) {
	for (void* b = vector_at(v, l); b != vector_at(v, r); b = vector_next(v, b))
		(*v->deleter)(*(void**)b);
}

bool vector_reserve(vector_t* v, size_t len) {
	if (len < v->capacity)
		return false;

	return vector_realloc(v, len);
}

vector_t* vector_init(size_t start_len, size_t element_size, vector_del_f* deleter) {
	vector_t* v = malloc(sizeof(vector_t));

	if (v) {
		start_len = MAX(start_len, VECTOR_MINSIZE);

		v->data = NULL; v->size = 0;
		v->blksz = element_size;
		v->deleter = deleter;

		if (!element_size || !vector_realloc(v, start_len))
			v = 0, free(v);
	}
	else error_code(ERROR_ALLOCATION);

	return v;
}

vector_t* vector_copy(const vector_t* v) {
	vector_t* k = vector_init(v->capacity, v->blksz, v->deleter);

	if (!memcpy(k->data, v->data, v->size * v->blksz)) {
		vector_free(k);
		error_set("could not copy vector, memcpy failed");
		return NULL;
	}

	k->size = v->size;
	return k;
}

void vector_free(vector_t* v) {
	if (v->deleter)
		vector_call_deletera(v);

	if (v->capacity)
		free(v->data);

	free(v);
}

bool vector_insert(vector_t* v, size_t index, void* val) {
	if (v->size+1 > v->capacity)
		if (!vector_realloc(v, VECTOR_GROWTH(v->capacity)))
			return error_set("could not realloc vector for insertion"), false;

	char* o = v->data + (index * v->blksz);

	if (!memmove(o + v->blksz, o, v->blksz * (v->size - index)))
		return error_set("could not insert into vector"), false;

	memcpy(o, &val, v->blksz); /* don't feel like checking */
	v->size++;

	return true;
}

bool vector_remove(vector_t* v, size_t index) {
	if (v->deleter)
		vector_call_deletere(v, index);

	char* i = v->data + (index * v->blksz);

	if (!memmove(i, i + v->blksz, v->blksz * (v->size - index)))
		return error_set("could not remove from vector"), false;

	v->size--;
	return true;
}

bool vector_append(vector_t* v, const void* vals, size_t val_count) {
	size_t new_count = v->size + val_count;

	if (v->capacity < new_count) {
		size_t amnt = v->capacity;

		do amnt = VECTOR_GROWTH(amnt);
		while (amnt < new_count);

		if (!vector_realloc(v, amnt))
			return error_set("could not resize vector in order to append to vector"), false;
	}

	if (!memcpy(v->data + (v->size * v->blksz), /* & */ vals, v->blksz * val_count))
		return error_set("could not append to vector"), false;

	v->size = new_count;

	return true;
}

bool vector_push_back(vector_t* v, void* val) {
	return vector_append(v, &val, 1);
}

void vector_pop(vector_t* v, size_t i) {
	ssize_t diff = v->size - i;
	if (diff < 0)
		return;

	if (v->deleter)
		vector_call_deleteri(v, diff, i);

	v->size = v->size - i;
}

bool vector_eq(vector_t* v1, vector_t* v2) {
	if (v1->size != v2->size || v1->blksz != v2->blksz)
		return false;

	for (size_t i = 0; i < v1->size * v1->blksz; i++)
		if (*(char*)(v1->data + i) != *(char*)(v2->data + i))
			return false;

	return true;
}

void* vector_set(vector_t* v, size_t index, void* val) {
	if (v->deleter)
		vector_call_deletere(v, index);

	return memcpy(v->data + (v->blksz * index), &val, v->blksz);
}

void vector_clear(vector_t* v) {
	if (v->deleter)
		vector_call_deletera(v);

	v->size = 0;
}

void vector_swap(vector_t* v1, vector_t* v2) {
	assert(v1->blksz == v2->blksz);
	ssize_t cap = MIN(v1->capacity, v2->capacity);
	ssize_t byt = cap * v1->blksz;

	void* tmp = malloc(byt);
	memcpy(tmp, v1->data, byt);
	memcpy(v1->data, v2->data, byt);
	memcpy(v2->data, tmp, byt);
	free(tmp);

	v1->size ^= v2->size;
	v2->size ^= v1->size;
	v1->size ^= v2->size;
}
