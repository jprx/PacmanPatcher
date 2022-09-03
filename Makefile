.PHONY: all
all: patch test_patch

patch: patch.c Makefile
	@gcc patch.c -o patch
	@echo "CC $@"

test_patch: test_patch.c Makefile
	@gcc test_patch.c -o test_patch
	@echo "CC $@"

.PHONY: clean
clean:
	rm -f patch test_patch
