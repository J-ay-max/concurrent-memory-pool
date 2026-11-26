#pragma once
#include<iostream>
using namespace std;


//Fixed-size object pool
//template<size_t N>
//class ObjectPool
//{};


template<class T>
class ObjectPool
{
public:
	
	T* New()
	{
		T* obj = nullptr;
		//Reuse returned memory blocks first
		if (_freelist)
		{
			void* next = *((void**)_freelist);
			obj = (T*)_freelist;
			_freelist = next;
			
		}
		else
		{
			// When remaining memory cannot hold one object, allocate a new large block
			if (_remainBytes < sizeof(T))
			{
				_remainBytes = 128 * 1024;
				_memory = (char*)malloc(_remainBytes);
				if (_memory == nullptr)
				{
					throw bad_alloc();
				}
			}
			// Carve one object from the current block (whether just allocated or still has room)
			obj = (T*)_memory;
			size_t objsize = sizeof(T) < sizeof(void*) ? sizeof(void*) : sizeof(T);
			_memory += objsize;
			_remainBytes -= objsize;
		}
		//Placement new: explicitly invoke the constructor
		new(obj)T;
		return obj;

	}
	void Delete(T* obj)
	{
		//Explicitly invoke the destructor
		obj->~T();
		//Link released objects into the free list
		if (_freelist==nullptr)
		{
			_freelist = obj;
			*(void**)obj = nullptr;
		}
		else
		{
			//Head insert
			*(void**)obj = _freelist;
			_freelist = obj;

		}
	}
private:
	char* _memory = nullptr;//pointer to the large memory block
	size_t _remainBytes = 0;//remaining bytes in the block
	void* _freelist=nullptr;//head of the free list built from returned blocks
	
};