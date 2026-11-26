#pragma once

#include "common.h"

// Singleton pattern
class CentralCache
{
public:
	static CentralCache* GetInstance()
	{
		return &_sInst;
	}

	// Fetch a non-empty span
	Span* GetOneSpan(SpanList& list, size_t byte_size);

	// Fetch a batch of objects from the central cache for this thread cache
	size_t FetchRangeObj(void*& start, void*& end, size_t batchNum, size_t size);

	// Release a batch of objects back to their spans
	void ReleaseListToSpans(void* start, size_t byte_size);
private:
	SpanList _spanLists[NFREELIST];

private:
	CentralCache()
	{
	}

	CentralCache(const CentralCache&) = delete;

	static CentralCache _sInst;
};