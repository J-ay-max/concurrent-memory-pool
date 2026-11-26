#pragma once
#pragma once

#include "common.h"
#include "ObjectPool.h"
#include "PageMap.h"

class PageCache
{
public:
	static PageCache* GetInstance()
	{
		return &_sInst;
	}

	// Look up the span that owns an object
	Span* MapObjectToSpan(void* obj);

	// Release a free span back to the page cache and coalesce neighbors
	void ReleaseSpanToPageCache(Span* span);

	// Fetch a span of K pages
	Span* NewSpan(size_t k);

	std::mutex _pageMtx;
private:
	SpanList _spanLists[NPAGES];
	ObjectPool<Span> _spanPool;

	//std::unordered_map<PAGE_ID, Span*> _idSpanMap;
	//std::map<PAGE_ID, Span*> _idSpanMap;
	TCMalloc_PageMap1<32 - PAGE_SHIFT> _idSpanMap;

	PageCache()
	{
	}
	PageCache(const PageCache&) = delete;


	static PageCache _sInst;
};