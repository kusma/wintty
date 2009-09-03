#include <stddef.h>

typedef unsigned int hash_t;

inline hash_t get_hash(const char *ptr, size_t len)
{
	hash_t hash = 2166136261UL;
	assert(len > 0);
	do {
		hash *= 16777619UL;
		hash ^= *ptr++;
	} while (--len);
	return hash;
}

struct line
{
	
};

struct line *cache_get_line(hash_t h)
{
	if (line_cache[])
}
