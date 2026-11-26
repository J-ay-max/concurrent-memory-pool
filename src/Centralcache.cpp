#include "Centralcache.h"
#include "Pagecache.h"

CentralCache CentralCache::_sInst;

// Fetch a non-empty span
Span* CentralCache::GetOneSpan(SpanList& list, size_t size)
{
	// Check whether any span in the list still has free objects
	Span* it = list.Begin();
	while (it != list.End())
	{
		if (it->_freeList != nullptr)
		{
			return it;
		}
		else
		{
			it = it->_next;
		}
	}

	// Release the bucket lock first so returning threads are not blocked
	list._mtx.unlock();

	// No free span available; request one from the page cache
	PageCache::GetInstance()->_pageMtx.lock();
	Span* span = PageCache::GetInstance()->NewSpan(SizeClass::NumMovePage(size));
	span->_isUse = true;
	span->_objSize = size;
	PageCache::GetInstance()->_pageMtx.unlock();

	// Split the fetched span without a lock: no other thread can reach it yet

	// Compute the span's start address and total size in bytes
	char* start = (char*)(span->_pageId << PAGE_SHIFT);
	size_t bytes = span->_n << PAGE_SHIFT;
	char* end = start + bytes;

	// Carve the block into objects and link them into a free list
	// 1. Cut one object first to serve as the list head (for O(1) tail insert)
	span->_freeList = start;
	start += size;
	void* tail = span->_freeList;
	int i = 1;
	while (start < end)
	{
		++i;
		NextObj(tail) = start;
		tail = NextObj(tail); // tail = start;
		start += size;
	}

	NextObj(tail) = nullptr;

	// 1. Conditional breakpoint
	// 2. Suspected infinite loop: break to inspect the current line
	//int j = 0;
	//void* cur = span->_freeList;
	//while (cur)
	//{
	//	cur = NextObj(cur);
	//	++j;
	//}

	//if (j != (bytes / size))
	//{
	//	int x = 0;
	//}

	// Only take the lock when attaching the split span back to the bucket
	list._mtx.lock();
	list.PushFront(span);

	return span;
}

// Fetch a batch of objects from the central cache for this thread cache
size_t CentralCache::FetchRangeObj(void*& start, void*& end, size_t batchNum, size_t size)
{
	size_t index = SizeClass::Index(size);
	_spanLists[index]._mtx.lock();

	Span* span = GetOneSpan(_spanLists[index], size);
	assert(span);
	assert(span->_freeList);

	// Take batchNum objects from the span
	// If fewer than batchNum remain, take as many as available
	start = span->_freeList;
	end = start;
	size_t i = 0;
	size_t actualNum = 1;
	while (i < batchNum - 1 && NextObj(end) != nullptr)
	{
		end = NextObj(end);
		++i;
		++actualNum;
	}
	span->_freeList = NextObj(end);
	NextObj(end) = nullptr;
	span->_useCount += actualNum;

	//// Conditional breakpoint
	int j = 0;
	void* cur = start;
	while (cur)
	{
		cur = NextObj(cur);
		++j;
	}

	if (j != actualNum)
	{
		int x = 0;
	}

	_spanLists[index]._mtx.unlock();

	return actualNum;
}

void CentralCache::ReleaseListToSpans(void* start, size_t size)
{
	size_t index = SizeClass::Index(size);
	_spanLists[index]._mtx.lock();
	while (start)
	{
		void* next = NextObj(start);

		Span* span = PageCache::GetInstance()->MapObjectToSpan(start);
		NextObj(start) = span->_freeList;
		span->_freeList = start;
		span->_useCount--;

		// All carved objects of this span have been returned
		// Recycle the span to the page cache, which can then try coalescing adjacent pages
		if (span->_useCount == 0)
		{
			_spanLists[index].Erase(span);
			span->_freeList = nullptr;
			span->_next = nullptr;
			span->_prev = nullptr;

			// Use the page cache lock when releasing a span back
			// Release the bucket lock at this point
			_spanLists[index]._mtx.unlock();

			PageCache::GetInstance()->_pageMtx.lock();
			PageCache::GetInstance()->ReleaseSpanToPageCache(span);
			PageCache::GetInstance()->_pageMtx.unlock();

			_spanLists[index]._mtx.lock();
		}

		start = next;
	}

	_spanLists[index]._mtx.unlock();
}