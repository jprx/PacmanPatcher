#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

// Information about the supported MacOS versions of this patch utility:
#define NAME_STRING_ORIGINAL    "DEVELOPMENT_ARM64_T"
#define NAME_STRING_NEW         "PACMANPATCH_ARM64_T"
#define NAME_STRING_NUM_EXPECTED ((2))

// General arm64/ macho stuff:
#define ARM64_INST_LEN ((4))
#define MACHO_HEADER "\xCF\xFA\xED\xFE"
#define MACHO_HEADER_LEN ((4))

// --------------- Begin patch set 1 ---------------
/*
XNU writes to PMCR0 with the following pattern:
08 80 86 52  mov   w8,#0x3400
08 e0 a0 72  movk  w8,#0x700, LSL #16
08 f0 19 d5  msr   sreg(0x3, 0x1, c0xf, c0x0, 0x0),x8
*/
#define PMCR0_PATCH_SET_1_FIND \
	"\x08\x80\x86\x52" \
	"\x08\xe0\xa0\x72" \
	"\x08\xf0\x19\xd5"

#define PMCR0_PATCH_SET_1_LEN ((3 * ARM64_INST_LEN))

/*
Make sure bit 30 is set:
08 80 86 52  mov   w8,#0x3400
08 e0 a8 72  movk  w8,#0x4700, LSL #16
08 f0 19 d5  msr   sreg(0x3, 0x1, c0xf, c0x0, 0x0),x8
*/
#define PMCR0_PATCH_SET_1_WITH \
	"\x08\x80\x86\x52" \
	"\x08\xe0\xa8\x72" \
	"\x08\xf0\x19\xd5"

#define PMCR0_PATCH_SET_1_WITH_LEN ((3 * ARM64_INST_LEN))

#define PMCR0_PATCH_SET_1_NAME "PMCR0 Patch"

#define PMCR0_PATCH_SET_1_NUM_EXPECTED ((6))

// --------------- Begin patch set 2 ---------------
// Body of disable_dc_mva_ops on T8132
/*
0ffc3cd5   mrs     x15, acfg_el1
ef017db2   orr     x15, x15, #0x8
0ffc1cd5   msr     acfg_el1, x15
*/
#define ACFG_PATCH_SET \
	"\x0f\xfc\x3c\xd5" \
	"\xef\x01\x7d\xb2" \
	"\x0f\xfc\x1c\xd5"

#define ACFG_PATCH_SET_LEN ((3 * ARM64_INST_LEN))

/*
Make sure bit 3 is 0
0ffc3cd5   mrs     x15, acfg_el1
eff97c92   and     x15, x15, #0xfffffffffffffff7
0ffc1cd5   msr     acfg_el1, x15
*/
#define ACFG_PATCH_SET_WITH \
	"\x0f\xfc\x3c\xd5" \
	"\xef\xf9\x7c\x92" \
	"\x0f\xfc\x1c\xd5"

#define ACFG_PATCH_SET_WITH_LEN ((ACFG_PATCH_SET_LEN))

#define ACFG_PATCH_SET_NUM_EXPECTED ((2))

#define ACFG_PATCH_SET_NUM_NAME "ACFG_EL1 Patch"

// --------------- Begin patch set 3 ---------------

// Body of disable_dc_mva_ops on cores using HID4/EHID4
/*
2ef438d5   mrs     x14, s3_0_c15_c4_1
02000014   b       0xfffffe000734561c
0ef438d5   mrs     x14, hid4
ce0175b2   orr     x14, x14, #0x800
*/
#define HID_PATCH_SET1 \
	"\x2e\xf4\x38\xd5" \
	"\x02\x00\x00\x14" \
	"\x0e\xf4\x38\xd5" \
	"\xce\x01\x75\xb2"

#define HID_PATCH_SET1_LEN ((4 * ARM64_INST_LEN))

/*
Make sure bit 11 is 0
2ef438d5   mrs     x14, s3_0_c15_c4_1
02000014   b       0xfffffe000734561c
0ef438d5   mrs     x14, hid4
cef97492   and     x14, x14, #0xfffffffffffff7ff
*/
#define HID_PATCH_SET1_WITH \
	"\x2e\xf4\x38\xd5" \
	"\x02\x00\x00\x14" \
	"\x0e\xf4\x38\xd5" \
	"\xce\xf9\x74\x92"

#define HID_PATCH_SET1_WITH_LEN ((HID_PATCH_SET1_LEN))

#define HID_PATCH_SET1_NUM_EXPECTED ((2))

#define HID_PATCH_SET1_NUM_NAME "HID4/ EHID4 Patch 1"


// --------------- Begin patch set 4 ---------------

// Body of CleanPoC_DcacheRegion_Force_nopreempt on old kernels
/*
This sets bit 11 to 1
ce0175b2   orr     x14, x14, #0x800
6f0000b5   cbnz    x15, 0xfffffe00072d5600
2ef418d5   msr     s3_0_c15_c4_1, x14
02000014   b       0xfffffe00072d5604
0ef418d5   msr     hid4, x14
*/
#define HID_PATCH_SET2 \
	"\xce\x01\x75\xb2" \
	"\x6f\x00\x00\xb5" \
	"\x2e\xf4\x18\xd5" \
	"\x02\x00\x00\x14" \
	"\x0e\xf4\x18\xd5"

#define HID_PATCH_SET2_LEN ((5 * ARM64_INST_LEN))

/*
Make sure bit 11 is 0
cef97492   and     x14, x14, #0xfffffffffffff7ff
6f0000b5   cbnz    x15, 0xfffffe00072d5600
2ef418d5   msr     s3_0_c15_c4_1, x14
02000014   b       0xfffffe00072d5604
0ef418d5   msr     hid4, x14
*/
#define HID_PATCH_SET2_WITH \
	"\xce\xf9\x74\x92" \
	"\x6f\x00\x00\xb5" \
	"\x2e\xf4\x18\xd5" \
	"\x02\x00\x00\x14" \
	"\x0e\xf4\x18\xd5"

#define HID_PATCH_SET2_WITH_LEN ((HID_PATCH_SET2_LEN))

#define HID_PATCH_SET2_NUM_EXPECTED ((1))

#define HID_PATCH_SET2_NUM_NAME "HID4/ EHID4 Patch 2"

/*
 * find_and_replace
 * Find a binary substring in the target region, and replace any occurances of it with something else.
 *
 * num_expected: Number of expected occurances. If this is 0, we do just patch all that we find.
 */
void find_and_replace(uint8_t *target, size_t target_len, uint8_t *find, size_t find_len, uint8_t *with, size_t with_len, int64_t write_offset, uint64_t num_expected, char *patch_name, int64_t stride) {
	uint8_t *cursor = target;

	uint64_t num_hits = 0;

	while (cursor < target + target_len) {
		if (memcmp(cursor, find, find_len) == 0) {
			num_hits++;
			printf("Patching %s\n", patch_name);

			for (int i = 0; i < with_len; i+=4) {
				int offset_idx = i + write_offset;
				printf("0x%llX:\t0x%X  =>  0x%X\n",
					(uint64_t)(cursor + offset_idx) - (uint64_t)target, // Location of patch
					*(uint32_t *)((uint64_t)(cursor + offset_idx)), // Original contents
					((uint32_t *)with)[i >> 2] // New contents
				);
			}

			memcpy(cursor + write_offset, with, with_len);
		}

		if (num_expected != 0) {
			if (num_hits > num_expected) {
				fprintf(stderr, "Found more hits than expected for a patch set. Your kernel might be newer than what we expect. Please update the patch utility and double-check the patch sets are correct for your kernel.\n");
				exit(EXIT_FAILURE);
			}
		}

		cursor += stride;
	}

	if (num_expected != 0) {
		if (num_hits != num_expected) {
			printf("Warning: Found fewer hits than expected for patch %s (got %llu, expected %llu). This might already be a patched kernel.\n", patch_name, num_hits, num_expected);
		}
	}
}

int main(int argc, char **argv) {
	size_t file_size = 0;
	struct stat kernel_stats;

	if (argc != 2) {
		fprintf(stderr, "Usage: %s [kernel binary to patch]\n", argv[0]);
		return EXIT_FAILURE;
	}

	int kernel_fd = open(argv[1], O_RDWR);

	if (-1 == kernel_fd) {
		fprintf(stderr, "Failed to open kernel binary %s (%s)\n", argv[1], strerror(errno));
		return EXIT_FAILURE;
	}

	if (-1 == fstat(kernel_fd, &kernel_stats)) {
		fprintf(stderr, "Failed to call fstat on kernel binary (%s)\n", strerror(errno));
		return EXIT_FAILURE;
	}

	file_size = kernel_stats.st_size;
	// printf("%s kernel binary is %zu bytes\n", argv[1], file_size);

	uint8_t *patch_kernel = mmap(NULL, file_size, PROT_READ | PROT_WRITE, MAP_SHARED, kernel_fd, 0);

	if (MAP_FAILED == patch_kernel) {
		fprintf(stderr, "Failed to mmap the kernel (%s)\n", strerror(errno));
		return EXIT_FAILURE;
	}

	if (strlen(NAME_STRING_ORIGINAL) != strlen(NAME_STRING_NEW)) {
		fprintf(stderr, "New name string (%s) isn't the same length as the original (%s)!\n",
			NAME_STRING_NEW,
			NAME_STRING_ORIGINAL
		);
		return EXIT_FAILURE;
	}

	if (0 != memcmp(MACHO_HEADER, patch_kernel, MACHO_HEADER_LEN)) {
		fprintf(stderr, "This (%s) isn't a macho file...\n", argv[1]);
		return EXIT_FAILURE;
	}

	// Patch the kernel name (for `uname -sra` etc)
	find_and_replace(
		patch_kernel,
		file_size,
		(uint8_t *)&NAME_STRING_ORIGINAL,
		strlen(NAME_STRING_ORIGINAL),
		(uint8_t *)&NAME_STRING_NEW,
		strlen(NAME_STRING_NEW),
		0,
		NAME_STRING_NUM_EXPECTED,
		"Kernel name",

		// Need to search byte by byte
		1
	);

	// Enable EL0 access to Apple PMC regs
	find_and_replace(
		patch_kernel,
		file_size,
		(uint8_t *)&PMCR0_PATCH_SET_1_FIND,
		PMCR0_PATCH_SET_1_LEN,
		(uint8_t *)&PMCR0_PATCH_SET_1_WITH,
		PMCR0_PATCH_SET_1_WITH_LEN,
		0,
		PMCR0_PATCH_SET_1_NUM_EXPECTED,
		PMCR0_PATCH_SET_1_NAME,

		// Increase search in chunks of instruction granularity
		ARM64_INST_LEN
	);

	// Remove any code that disables DC MVA ops with ACFG_EL1
	find_and_replace(
		patch_kernel,
		file_size,
		(uint8_t *)&ACFG_PATCH_SET,
		ACFG_PATCH_SET_LEN,
		(uint8_t *)&ACFG_PATCH_SET_WITH,
		ACFG_PATCH_SET_WITH_LEN,
		0,
		ACFG_PATCH_SET_NUM_EXPECTED,
		ACFG_PATCH_SET_NUM_NAME,
		ARM64_INST_LEN
	);

	// Remove any code that disables DC MVA ops with HID4/ EHID4
	find_and_replace(
		patch_kernel,
		file_size,
		(uint8_t *)&HID_PATCH_SET1,
		HID_PATCH_SET1_LEN,
		(uint8_t *)&HID_PATCH_SET1_WITH,
		HID_PATCH_SET1_WITH_LEN,
		0,
		HID_PATCH_SET1_NUM_EXPECTED,
		HID_PATCH_SET1_NUM_NAME,
		ARM64_INST_LEN
	);

	// Remove any code that disables DC MVA ops with HID4/ EHID4 on old kernels
	find_and_replace(
		patch_kernel,
		file_size,
		(uint8_t *)&HID_PATCH_SET2,
		HID_PATCH_SET2_LEN,
		(uint8_t *)&HID_PATCH_SET2_WITH,
		HID_PATCH_SET2_WITH_LEN,
		0,
		HID_PATCH_SET2_NUM_EXPECTED,
		HID_PATCH_SET2_NUM_NAME,
		ARM64_INST_LEN
	);

	msync(patch_kernel, file_size, MS_SYNC);
	munmap(patch_kernel, file_size);
	close(kernel_fd);

	return EXIT_SUCCESS;
}
