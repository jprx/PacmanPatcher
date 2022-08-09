.PHONY: all
all: patch test_patch

patch: patch.c Makefile
	gcc patch.c -o patch

test_patch: test_patch.c Makefile
	gcc test_patch.c -o test_patch

.PHONY: clean
clean:
	rm -f patch test_patch
