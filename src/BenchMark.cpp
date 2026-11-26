#include"ConcurrentAlloc.h"

// ntimes: number of alloc+free operations per round
// rounds: number of repetitions
void BenchmarkMalloc(size_t ntimes, size_t nworks, size_t rounds)
{
	std::vector<std::thread> vthread(nworks);
	std::atomic<size_t> malloc_costtime = 0;
	std::atomic<size_t> free_costtime = 0;

	for (size_t k = 0; k < nworks; ++k)
	{
		vthread[k] = std::thread([&, k]() {
			std::vector<void*> v;
			v.reserve(ntimes);

			for (size_t j = 0; j < rounds; ++j)
			{
				size_t begin1 = clock();
				for (size_t i = 0; i < ntimes; i++)
				{
					v.push_back(malloc(16));
					//v.push_back(malloc((16 + i) % 8192 + 1));
				}
				size_t end1 = clock();

				size_t begin2 = clock();
				for (size_t i = 0; i < ntimes; i++)
				{
					free(v[i]);
				}
				size_t end2 = clock();
				v.clear();

				malloc_costtime += (end1 - begin1);
				free_costtime += (end2 - begin2);
			}
			});
	}

	for (auto& t : vthread)
	{
		t.join();
	}

	printf("%zu threads x %zu rounds, malloc %zu times each: %u ms\n",
		nworks, rounds, ntimes, (unsigned)malloc_costtime.load());

	printf("%zu threads x %zu rounds, free %zu times each: %u ms\n",
		nworks, rounds, ntimes, (unsigned)free_costtime.load());

	printf("%u threads, malloc&free %zu times each, total: %u ms\n",
		(unsigned)nworks, (size_t)(nworks * rounds * ntimes), (unsigned)(malloc_costtime.load() + free_costtime.load()));
}


// ops-per-round, thread-count, rounds
void BenchmarkConcurrentMalloc(size_t ntimes, size_t nworks, size_t rounds)
{
	std::vector<std::thread> vthread(nworks);
	std::atomic<size_t> malloc_costtime = 0;
	std::atomic<size_t> free_costtime = 0;

	for (size_t k = 0; k < nworks; ++k)
	{
		vthread[k] = std::thread([&]() {
			std::vector<void*> v;
			v.reserve(ntimes);

			for (size_t j = 0; j < rounds; ++j)
			{
				size_t begin1 = clock();
				for (size_t i = 0; i < ntimes; i++)
				{
					v.push_back(ConcurrentAlloc(16));
					//v.push_back(ConcurrentAlloc((16 + i) % 8192 + 1));
				}
				size_t end1 = clock();

				size_t begin2 = clock();
				for (size_t i = 0; i < ntimes; i++)
				{
					ConcurrentFree(v[i]);
				}
				size_t end2 = clock();
				v.clear();

				malloc_costtime += (end1 - begin1);
				free_costtime += (end2 - begin2);
			}
			});
	}

	for (auto& t : vthread)
	{
		t.join();
	}

	printf("%zu threads x %zu rounds, ConcurrentAlloc %zu times each: %u ms\n",
		nworks, rounds, ntimes, (unsigned)malloc_costtime.load());

	printf("%zu threads x %zu rounds, ConcurrentFree %zu times each: %u ms\n",
		nworks, rounds, ntimes, (unsigned)free_costtime.load());

	printf("%u threads, ConcurrentAlloc&Free %zu times each, total: %u ms\n",
		(unsigned)nworks, (size_t)(nworks * rounds * ntimes), (unsigned)(malloc_costtime.load() + free_costtime.load()));
}

int main()
{
	size_t n = 100000;
	cout << "==========================================================" << endl;
	BenchmarkConcurrentMalloc(n, 4, 10);
	cout << endl << endl;

	BenchmarkMalloc(n, 4, 10);
	cout << "==========================================================" << endl;

	return 0;
}