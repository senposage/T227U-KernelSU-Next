// SPDX-License-Identifier: GPL-2.0-only
/* Link against the ARM64 kernel LZ4 objects and run in userspace. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum {
	PAGE_BYTES = 4096,
	PAGE_COUNT = 1024,
	PREFIX_BYTES = 997,
};

/* The in-kernel API takes a caller-owned LZ4_MEM_COMPRESS workspace. */
static uint64_t workspace[8192];
static uint64_t dict_template[8192];
static char dictionary[64 * 1024];

int LZ4_compress_default(const char *src, char *dst, int src_len,
			 int dst_capacity, void *workmem);
int LZ4_decompress_safe(const char *src, char *dst, int src_len,
			int dst_capacity);
int LZ4_decompress_safe_partial(const char *src, char *dst, int src_len,
				int target_len, int dst_capacity);
int LZ4_loadDict(void *stream, const char *dict, int dict_size);
int LZ4_compress_fast_continue(void *stream, const char *src, char *dst,
			       int src_len, int dst_capacity, int acceleration);
int LZ4_decompress_safe_usingDict(const char *src, char *dst, int src_len,
				  int dst_capacity, const char *dict, int dict_size);

static uint32_t next_random(uint32_t *state)
{
	*state ^= *state << 13;
	*state ^= *state >> 17;
	*state ^= *state << 5;
	return *state;
}

static void fill_page(char *page, unsigned int index)
{
	uint32_t state = index + 1;
	unsigned int offset;

	for (offset = 0; offset < PAGE_BYTES; offset++) {
		switch (index % 4) {
		case 0:
			page[offset] = index;
			break;
		case 1:
			page[offset] = (offset + index) % 31;
			break;
		case 2:
			page[offset] = next_random(&state) & 15;
			break;
		default:
			page[offset] = next_random(&state);
			break;
		}
	}
}

static int test_dictionary(unsigned int dict_size)
{
	char input[PAGE_BYTES], restored[PAGE_BYTES];
	char packed[PAGE_BYTES + 64], reload_packed[PAGE_BYTES + 64];
	uint32_t state = 17;
	unsigned int i, offset, dict_refs = 0;
	int bytes, reload_bytes;

	for (i = 0; i < dict_size; i++)
		dictionary[i] = next_random(&state);
	memset(dict_template, 0, sizeof(dict_template));
	if (LZ4_loadDict(dict_template, dictionary, dict_size) != (int)dict_size)
		return 1;

	for (i = 0; i < 96; i++) {
		offset = i * 251 % (dict_size - PAGE_BYTES + 1);
		memcpy(input, dictionary + offset, sizeof(input));
		input[0] ^= i;
		memcpy(workspace, dict_template, sizeof(workspace));
		bytes = LZ4_compress_fast_continue(workspace, input, packed,
						    PAGE_BYTES, sizeof(packed), 1);
		if (bytes <= 0) {
			fprintf(stderr, "Dictionary compression failed: size=%u page=%u\n",
				dict_size, i);
			return 1;
		}

		memset(workspace, 0, sizeof(workspace));
		if (LZ4_loadDict(workspace, dictionary, dict_size) != (int)dict_size)
			return 1;
		reload_bytes = LZ4_compress_fast_continue(workspace, input,
				 reload_packed, PAGE_BYTES, sizeof(reload_packed), 1);
		if (bytes != reload_bytes || memcmp(packed, reload_packed, bytes)) {
			fprintf(stderr, "Dictionary template differs: size=%u page=%u\n",
				dict_size, i);
			return 1;
		}

		if (LZ4_decompress_safe(packed, restored, bytes, PAGE_BYTES) !=
		    PAGE_BYTES || memcmp(input, restored, PAGE_BYTES))
			dict_refs++;

		if (LZ4_decompress_safe_usingDict(packed, restored, bytes,
				PAGE_BYTES, dictionary, dict_size) != PAGE_BYTES ||
		    memcmp(input, restored, PAGE_BYTES)) {
			fprintf(stderr, "Dictionary decode differs: size=%u page=%u\n",
				dict_size, i);
			return 1;
		}
	}
	if (!dict_refs) {
		fprintf(stderr, "Test pages used no dictionary references: size=%u\n",
			dict_size);
		return 1;
	}
	return 0;
}

int main(void)
{
	char input[PAGE_BYTES], restored[PAGE_BYTES], compressed[PAGE_BYTES + 64];
	char inplace[PAGE_BYTES + 64];
	uint32_t digest = UINT32_C(2166136261);
	unsigned int index, offset, inplace_count = 0;
	int bytes, restored_len;

	for (index = 0; index < PAGE_COUNT; index++) {
		fill_page(input, index);
		bytes = LZ4_compress_default(input, compressed, PAGE_BYTES,
					     sizeof(compressed), workspace);
		if (bytes <= 0)
			goto failed;
		restored_len = LZ4_decompress_safe(compressed, restored,
						   bytes, sizeof(restored));
		if (restored_len != PAGE_BYTES || memcmp(input, restored, PAGE_BYTES))
			goto failed;
		restored_len = LZ4_decompress_safe_partial(compressed, restored,
						       bytes, PREFIX_BYTES,
						       sizeof(restored));
		if (restored_len < PREFIX_BYTES ||
		    memcmp(input, restored, PREFIX_BYTES))
			goto failed;
		if (bytes < PAGE_BYTES) {
			char *tail = inplace + sizeof(inplace) - bytes;

			memcpy(tail, compressed, bytes);
			restored_len = LZ4_decompress_safe(tail, inplace, bytes,
							   PAGE_BYTES);
			if (restored_len != PAGE_BYTES ||
			    memcmp(input, inplace, PAGE_BYTES))
				goto failed;
			inplace_count++;
		}
		for (offset = 0; offset < (unsigned int)bytes; offset++) {
			digest ^= (unsigned char)compressed[offset];
			digest *= UINT32_C(16777619);
		}
	}
	printf("PASS pages=%u partial=%u inplace=%u digest=%08x\n",
	       PAGE_COUNT, PAGE_COUNT, inplace_count, digest);
	if (test_dictionary(16 * 1024) || test_dictionary(64 * 1024)) {
		fprintf(stderr, "LZ4 dictionary round trip or template mismatch\n");
		return 1;
	}
	puts("PASS dictionary pages=192 sizes=16KiB,64KiB template_matches_reload");
	return 0;

failed:
	fprintf(stderr, "LZ4 round trip failed on page %u\n", index);
	return 1;
}
