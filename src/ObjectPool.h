#pragma once
#include<iostream>
using namespace std;


//定长内存池
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
		//优先把还回来的内存块再次重复利用
		if (_freelist)
		{
			void* next = *((int**)_freelist);
			obj = (T*)_freelist;
			_freelist = next;
			
		}
		else
		{
			//剩余内存不够一个对像空间时，重新开辟大块空间
			if (_remainBytes < sizeof(T))
			{
				_remainBytes = 128 * 1024;
				_memory = (char*)malloc(_remainBytes);
				if (_memory == nullptr)
				{
					throw bad_alloc();
				}
				obj = (T*)_memory;
				size_t objsize = sizeof(T) < sizeof(void*) ? sizeof(void*) : sizeof(T);
				_memory += objsize;
				_remainBytes -= objsize;
		
			}
		}
		//定位new,显式调用构造函数初始化
		new(obj)T;
		return obj;

	}
	void Delete(T* obj)
	{
		//显式调用析构函数
		obj->~T();
		//将释放的对象链接到自由链表中
		if (_freelist==nullptr)
		{
			_freelist = obj;
			*(int**)obj = nullptr;
		}
		else
		{
			//头插
			*(int**)obj = _freelist;
			_freelist = obj;

		}
	}
private:
	char* _memory = nullptr;//指向大块内存的指针
	size_t _remainBytes = 0;//剩余内存大小
	void* _freelist=nullptr;//还回来过程中链接的自由链表的头指针
	
};