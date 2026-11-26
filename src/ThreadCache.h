#pragma once

#include "common.h"

class ThreadCache
{
public:
	// Allocate and free objects
	void* Allocate(size_t size);
	void Deallocate(void* ptr, size_t size);

	// Fetch objects from the central cache
	void* FetchFromCentralCache(size_t index, size_t size);

	// On free, return a batch to the central cache when the list is too long
	void ListTooLong(FreeList& list, size_t size);
private:
	FreeList _freeLists[NFREELIST];
};

// TLS thread-local storage: inline gives one shared entity across translation units (C++17).
inline thread_local ThreadCache* pTLSThreadCache = nullptr;