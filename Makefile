.PHONY: all
all: patch test_pmc test_flush

patch: patch.c Makefile
	@gcc patch.c -o patch
	@echo "CC $@"

test_pmc: test_pmc.c Makefile
	@gcc test_pmc.c -o test_pmc
	@echo "CC $@"

test_flush: test_flush.c util.s Makefile
	@gcc test_flush.c util.s -o test_flush
	@echo "CC $@"

.PHONY: clean
clean:
	$(RM) patch test_pmc test_flush
