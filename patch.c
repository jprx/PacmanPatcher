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
// Right now we just support 12.5.1 (xnu-8020.141.5~2)
// We support 12.4 as well (`git checkout 12.4`)
#define MACOS_VERSION "12.5.1"

#define NAME_STRING_ORIGINAL    "root:xnu-8020.141.5~2/DEVELOPMENT_ARM64_T"
#define NAME_STRING_NEW         "root:xnu-8020.141.5~2/PACMANPATCH_ARM64_T"
#define NAME_STRING_NUM_EXPECTED ((2))

// General arm64/ macho stuff:
#define ARM64_INST_LEN ((4))
#define MACHO_HEADER "\xCF\xFA\xED\xFE"
#define MACHO_HEADER_LEN ((4))

// --------------- Begin patch set 1 ---------------

#define CNTKCTL_PATCH_NAME "CNTKCTL"

/*
Original patterns look something like:

(some branch)
mrs x9,cntkctl_el1
orr x8,x9,x8, LSL #0x4
orr x8,x8,#0xf
msr cntkctl_el1,x8
*/
#define CNTKCTL_PATCH_SET \
	"\x09\xe1\x38\xd5" \
	"\x28\x11\x08\xaa" \
	"\x08\x0d\x40\xb2" \
	"\x08\xe1\x18\xd5"

#define CNTKCTL_PATCH_LEN ((4 * ARM64_INST_LEN))

/*
We want bits 8 and 9 set too. Need another instruction
so we just overwrite the previous branch (CNTKCTL_PATCH_OFFSET is -4)

In all cases (as of MacOS 12.4) that this occurs, the proceeding inst
is a branch that leads to a kernel panic condition. We never will hit that
panic (in good code) so we can assume that branch never runs and just overwrite it.

Not ideal, but then again, we are editing the kernel by hand here...

mrs x9,cntkctl_el1
orr x8,x9,x8, LSL #0x4
orr x8,x8,#0xf
orr x8,x8,#0x3 LSL #0x8
msr cntkctl_el1,x8
*/
#define CNTKCTL_PATCH_WITH \
	"\x09\xe1\x38\xd5" \
	"\x28\x11\x08\xaa" \
	"\x08\x0d\x40\xb2" \
	"\x08\x05\x78\xb2" \
	"\x08\xe1\x18\xd5"

#define CNTKCTL_PATCH_WITH_LEN ((5 * ARM64_INST_LEN))

#define CNTKCTL_PATCH_OFFSET ((-4))

// wfe_timeout_init, arm_init_idle_cpu, arm_init_cpu, and arm_init
#define CNTKCTL_PATCH_EXPECTED ((4))

// --------------- Begin patch set 2 ---------------

/*
Sometimes we write to PMCR0 with the following pattern:
08 80 86 52  mov   w8,#0x3400
08 e0 a0 72  movk  w8,#0x700, LSL #16
08 f0 19 d5  msr   sreg(0x3, 0x1, c0xf, c0x0, 0x0),x8
*/
#define PMCR0_PATCH_SET_1_FIND "\x08\x80\x86\x52\x08\xe0\xa0\x72\x08\xf0\x19\xd5"

#define PMCR0_PATCH_SET_1_LEN ((3 * ARM64_INST_LEN))

/*
We need bit 30 set, so we can just do this (1 byte patch):

08 80 86 52  mov   w8,#0x3400
08 e0 a8 72  movk  w8,#0x4700, LSL #16
08 f0 19 d5  msr   sreg(0x3, 0x1, c0xf, c0x0, 0x0),x8
*/
#define PMCR0_PATCH_SET_1_WITH "\x08\x80\x86\x52\x08\xe0\xa8\x72\x08\xf0\x19\xd5"

#define PMCR0_PATCH_SET_1_WITH_LEN ((3 * ARM64_INST_LEN))

#define PMCR0_PATCH_SET_1_NAME "PMCR0 Patch Set 1"

#define PMCR0_PATCH_SET_1_NUM_EXPECTED ((6))

// --------------- Begin patch set 3 ---------------

/*
Sometimes XNU does the following, which eventually loads x19 into PMCR0:

This is the most dangerous patch we support as it doesn't explicitly include a
write to the register in question. Future versions MAY break this!

Proceed with caution here.

73 80 86 52 mov  w19,#0x3403
13 e0 a0 72 movk w19,#0x700, LSL #16
*/
#define PMCR0_PATCH_SET_2_FIND "\x73\x80\x86\x52\x13\xe0\xa0\x72"

#define PMCR0_PATCH_SET_2_LEN ((2 * ARM64_INST_LEN))

/*
We need bit 30 set, so we can just do this (1 byte patch):

73 80 86 52 mov  w19,#0x3403
08 e0 a8 72 movk w8,#0x4700, LSL #16
*/
#define PMCR0_PATCH_SET_2_WITH "\x73\x80\x86\x52\x08\xe0\xa8\x72"

#define PMCR0_PATCH_SET_2_WITH_LEN ((2 * ARM64_INST_LEN))

#define PMCR0_PATCH_SET_2_NAME "PMCR0 Patch Set 2"

#define PMCR0_PATCH_SET_2_NUM_EXPECTED ((2))

// Note- we don't care about the write in mt_cpu_down as
// it just turns off the counters before idling. We don't care about
// the timers when the core is idle!

/*
 * Returns true of the substring is present in the image
 */
bool require_string(uint8_t *target, size_t target_len, uint8_t *find, size_t find_len) {
	uint8_t *cursor = target;

	while (cursor < target + target_len) {
		if (memcmp(cursor, find, find_len) == 0) {
			return true;
		}

		cursor ++;
	}

	return false;
}

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
				fprintf(stderr, "Found more hits than expected for a patch set. Your kernel might be newer than what we expect (%s). Please update the patch utility and double-check the patch sets are correct for your kernel.\n", MACOS_VERSION);
				exit(EXIT_FAILURE);
			}
		}

		cursor += stride;
	}

	if (num_expected != 0) {
		if (num_hits != num_expected) {
			printf("Warning: Found fewer hits than expected for patch %s. This might already be a patched kernel.\n", patch_name);
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

	// We require that the name string is present before making any patches
	if (!require_string(patch_kernel, file_size, (uint8_t *)&NAME_STRING_ORIGINAL, strlen(NAME_STRING_ORIGINAL))) {

		// This is a previously patched kernel image!
		if (require_string(patch_kernel, file_size, (uint8_t *)&NAME_STRING_NEW, strlen(NAME_STRING_NEW))) {
			printf("This binary has already been patched!\n");
			return EXIT_SUCCESS;
		}

		fprintf(stderr, "Your kernel does not contain the expected version string (%s)!\n", NAME_STRING_ORIGINAL);
		fprintf(stderr, "\nPlease update the PacmanPatcher utility to support your kernel.\n");
		fprintf(stderr, "\nOr, if you know what you are doing, you can probably just comment this warning out and things might still work (I recommend checking in a disassembler though- see the comments in the code for what the patches do)\n");
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

	// CNTKCTL_EL1 timer reg patch (turn on user timer bits)
	find_and_replace(
		patch_kernel,
		file_size,
		(uint8_t *)&CNTKCTL_PATCH_SET,
		CNTKCTL_PATCH_LEN,
		(uint8_t *)&CNTKCTL_PATCH_WITH,
		CNTKCTL_PATCH_WITH_LEN,

		// We write our patch 4 bytes backwards to overwrite the prior branch
		// (which in all 4 cases leads to a panic handler we will never hit)
		CNTKCTL_PATCH_OFFSET,
		CNTKCTL_PATCH_EXPECTED,
		CNTKCTL_PATCH_NAME,

		// Increase search in chunks of instruction granularity
		ARM64_INST_LEN
	);

	// Apple PMC timer reg patch 1 (turn on user timer bits)
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

	// Apple PMC timer reg patch 2 (turn on user timer bits)
	find_and_replace(
		patch_kernel,
		file_size,
		(uint8_t *)&PMCR0_PATCH_SET_2_FIND,
		PMCR0_PATCH_SET_2_LEN,
		(uint8_t *)&PMCR0_PATCH_SET_2_WITH,
		PMCR0_PATCH_SET_2_WITH_LEN,
		0,
		PMCR0_PATCH_SET_2_NUM_EXPECTED,
		PMCR0_PATCH_SET_2_NAME,

		// Increase search in chunks of instruction granularity
		ARM64_INST_LEN
	);

	msync(patch_kernel, file_size, MS_SYNC);
	munmap(patch_kernel, file_size);
	close(kernel_fd);

	return EXIT_SUCCESS;
}
