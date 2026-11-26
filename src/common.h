#pragma once

#include <iostream>
#include <vector>
#include <unordered_map>
#include <map>
#include <algorithm>

#include <time.h>
#include <assert.h>

#include <thread>
#include <mutex>
#include <atomic>

using std::cout;
using std::endl;

#ifdef _WIN32
#include <windows.h>
#else
// ...
#endif

static const size_t MAX_BYTES = 256 * 1024;
static const size_t NFREELIST = 208;
static const size_t NPAGES = 129;
static const size_t PAGE_SHIFT = 13;

#ifdef _WIN64
typedef unsigned long long PAGE_ID;
#elif _WIN32
typedef size_t PAGE_ID;
#else
// linux
#endif

// Allocate page-aligned blocks straight from the heap
inline static void* SystemAlloc(size_t kpage)
{
#ifdef _WIN32
	void* ptr = VirtualAlloc(0, kpage << 13, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#else
	// brk / mmap on Linux
#endif

	if (ptr == nullptr)
		throw std::bad_alloc();

	return ptr;
}


inline static void SystemFree(void* ptr)
{
#ifdef _WIN32
	VirtualFree(ptr, 0, MEM_RELEASE);
#else
	// sbrk / munmap etc.
#endif
}

static void*& NextObj(void* obj)
{
	return *(void**)obj;
}


// Free list managing carved small objects
class FreeList
{
public:
	void Push(void* obj)
	{
		assert(obj);

		// Head insert
		//*(void**)obj = _freeList;
		NextObj(obj) = _freeList;
		_freeList = obj;

		++_size;
	}

	void PushRange(void* start, void* end, size_t n)
	{
		NextObj(end) = _freeList;
		_freeList = start;

		// Test hook + conditional breakpoint
		/*int i = 0;
		void* cur = start;
		while (cur)
		{
			cur = NextObj(cur);
			++i;
		}

		if (n != i)
		{
			int x = 0;
		}*/

		_size += n;
	}

	void PopRange(void*& start, void*& end, size_t n)
	{
		assert(n <= _size);
		start = _freeList;
		end = start;

		for (size_t i = 0; i < n - 1; ++i)
		{
			end = NextObj(end);
		}

		_freeList = NextObj(end);
		NextObj(end) = nullptr;
		_size -= n;
	}

	void* Pop()
	{
		assert(_freeList);

		// Head removal
		void* obj = _freeList;
		_freeList = NextObj(obj);
		--_size;

		return obj;
	}

	bool Empty()
	{
		return _freeList == nullptr;
	}

	size_t& MaxSize()
	{
		return _maxSize;
	}

	size_t Size()
	{
		return _size;
	}

private:
	void* _freeList = nullptr;
	size_t _maxSize = 1;
	size_t _size = 0;
};

// Size-class alignment and mapping rules
class SizeClass
{
public:
	// Keep internal fragmentation at roughly 10% at most
	// [1,128]           8B alignment    freelist[0,16)
	// [129,1024]         16B alignment   freelist[16,72)
	// [1025,8K]          128B alignment  freelist[72,128)
	// [8K+1,64K]         1024B alignment freelist[128,184)
	// [64K+1,256K]       8K alignment    freelist[184,208)

	/*size_t _RoundUp(size_t size, size_t alignNum)
	{
		size_t alignSize;
		if (size % alignNum != 0)
		{
			alignSize = (size / alignNum + 1)*alignNum;
		}
		else
		{
			alignSize = size;
		}

		return alignSize;
	}*/
	// 1-8 
	static inline size_t _RoundUp(size_t bytes, size_t alignNum)
	{
		return ((bytes + alignNum - 1) & ~(alignNum - 1));
	}

	static inline size_t RoundUp(size_t size)
	{
		if (size <= 128)
		{
			return _RoundUp(size, 8);
		}
		else if (size <= 1024)
		{
			return _RoundUp(size, 16);
		}
		else if (size <= 8 * 1024)
		{
			return _RoundUp(size, 128);
		}
		else if (size <= 64 * 1024)
		{
			return _RoundUp(size, 1024);
		}
		else if (size <= 256 * 1024)
		{
			return _RoundUp(size, 8 * 1024);
		}
		else
		{
			return _RoundUp(size, 1 << PAGE_SHIFT);
		}
	}

	/*size_t _Index(size_t bytes, size_t alignNum)
	{
	if (bytes % alignNum == 0)
	{
	return bytes / alignNum - 1;
	}
	else
	{
	return bytes / alignNum;
	}
	}*/

	// 1 + 7  8
	// 2      9
	// ...
	// 8      15

	// 9 + 7 16
	// 10
	// ...
	// 16    23
	static inline size_t _Index(size_t bytes, size_t align_shift)
	{
		return ((bytes + (1 << align_shift) - 1) >> align_shift) - 1;
	}

	// Compute which free-list bucket a size maps to
	static inline size_t Index(size_t bytes)
	{
		assert(bytes <= MAX_BYTES);

		// number of lists per size range
		static int group_array[4] = { 16, 56, 56, 56 };
		if (bytes <= 128) {
			return _Index(bytes, 3);
		}
		else if (bytes <= 1024) {
			return _Index(bytes - 128, 4) + group_array[0];
		}
		else if (bytes <= 8 * 1024) {
			return _Index(bytes - 1024, 7) + group_array[1] + group_array[0];
		}
		else if (bytes <= 64 * 1024) {
			return _Index(bytes - 8 * 1024, 10) + group_array[2] + group_array[1] + group_array[0];
		}
		else if (bytes <= 256 * 1024) {
			return _Index(bytes - 64 * 1024, 13) + group_array[3] + group_array[2] + group_array[1] + group_array[0];
		}
		else {
			assert(false);
		}

		return -1;
	}

	// How many objects a thread cache fetches per batch
	static size_t NumMoveSize(size_t size)
	{
		assert(size > 0);

		// [2, 512] slow-start cap on objects moved per batch
		// Small objects -> higher batch cap
		// Large objects -> lower batch cap
		int num = MAX_BYTES / size;
		if (num < 2)
			num = 2;

		if (num > 512)
			num = 512;

		return num;
	}

	// Compute how many pages to request from the system
	// min object size: 8 bytes
	// ...
	// max object size: 256 KB
	static size_t NumMovePage(size_t size)
	{
		size_t num = NumMoveSize(size);
		size_t npage = num * size;

		npage >>= PAGE_SHIFT;
		if (npage == 0)
			npage = 1;

		return npage;
	}
};

// Span: a run of consecutive pages
struct Span
{
	PAGE_ID _pageId = 0; // page id of the span's first page
	size_t  _n = 0;      // number of pages

	Span* _next = nullptr;	// doubly-linked list pointer
	Span* _prev = nullptr;

	size_t _objSize = 0;  // size of carved small objects
	size_t _useCount = 0; // count of carved objects handed out to thread caches
	void* _freeList = nullptr;  // free list of carved objects

	bool _isUse = false;          // whether the span is in use
};

// Doubly-circular list with a sentinel head 
class SpanList
{
public:
	SpanList()
	{
		_head = new Span;
		_head->_next = _head;
		_head->_prev = _head;
	}

	Span* Begin()
	{
		return _head->_next;
	}

	Span* End()
	{
		return _head;
	}

	bool Empty()
	{
		return _head->_next == _head;
	}

	void PushFront(Span* span)
	{
		Insert(Begin(), span);
	}

	Span* PopFront()
	{
		Span* front = _head->_next;
		Erase(front);
		return front;
	}

	void Insert(Span* pos, Span* newSpan)
	{
		assert(pos);
		assert(newSpan);

		Span* prev = pos->_prev;
		// prev newspan pos
		prev->_next = newSpan;
		newSpan->_prev = prev;
		newSpan->_next = pos;
		pos->_prev = newSpan;
	}

	void Erase(Span* pos)
	{
		assert(pos);
		assert(pos != _head);

		// 1. Conditional breakpoint
		// 2. Inspect the stack frame
		/*if (pos == _head)
		{
		int x = 0;
		}*/

		Span* prev = pos->_prev;
		Span* next = pos->_next;

		prev->_next = next;
		next->_prev = prev;
	}

private:
	Span* _head;
public:
	std::mutex _mtx; // bucket lock
};