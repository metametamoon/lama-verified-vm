DEBUG_FILES=stack-dump-before data-dump-before extra-roots-dump-before heap-dump-before stack-dump-after data-dump-after extra-roots-dump-after heap-dump-after
TESTS=$(sort $(filter-out test111, $(basename $(wildcard test*.lama))))

LAMAC=lamac

.PHONY: check $(TESTS)


check: $(TESTS)

$(TESTS): %: %.lama
	@echo $@
	$(LAMAC) $@.lama -b
	# byterun $@.bc > $@.dis
	cat $@.input | ../build/vm $@.bc 2> /dev/null > $@.log && diff $@.log orig/$@.log --strip-trailing-cr

ctest111:
	@echo "regression/test111"
	@LAMA=../runtime $(LAMAC) test111.lama && cat test111.input | ./test111 > test111.log && diff test111.log orig/test111.log

clean:
	$(RM) test*.log *.s *.sm *~ $(TESTS) *.i $(DEBUG_FILES) test111
	$(MAKE) clean -C expressions
	$(MAKE) clean -C deep-expressions
