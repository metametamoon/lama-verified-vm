# TESTS=$(sort $(basename $(wildcard tests/test*.lama)))
T1=$(sort $(basename $(wildcard tests/*.lama)))
REGRESSION=$(sort $(basename $(wildcard regression/*.lama)))
EXECUTABLE=build/analyzer
TESTS=$(notdir $(T1))
LAMAC=lamac

$(EXECUTABLE): src/main.cpp src/bytefile.cpp
	mkdir -p build
	g++ -m32 -O3 -fstack-protector-all -Wall -Wextra -Werror -Wno-unused-variable -Wno-unused-parameter -o build/main.o -c $<
	g++ -m32 -O3 -fstack-protector-all -Wall -Wextra -Werror -o build/bytefile.o -c src/bytefile.cpp
	make -C src/runtime/ all
	g++ build/main.o src/runtime/gc.o src/runtime/runtime.o build/bytefile.o -o $(EXECUTABLE) -m32 -O3 -fstack-protector-all


.PHONY: test  regression benchmark

regression: $(REGRESSION)

benchmark: performance/Sort.lama $(EXECUTABLE)
	$(LAMAC) -b performance/Sort.lama
	mv Sort.bc build/Sort.bc
	$(EXECUTABLE) build/Sort.bc

$(REGRESSION): %: %.lama $(EXECUTABLE)
	@echo $@
	$(LAMAC) $@.lama -b
	mv $(notdir $@).bc regression/$(notdir $@).bc
	byterun $@.bc > $@.dis
	cat $@.input | $(EXECUTABLE) $@.bc  > $@.log && diff $@.log regression/orig/$(notdir $@).log --strip-trailing-cr

test: $(TESTS)



$(TESTS): %: $(EXECUTABLE)
	@echo $@
	lamac -b tests/$@.lama
	mkdir -p bytecodes
	byterun $@.bc > bytecodes/$@.dis
	mv $@.bc bytecodes/$@.bc
	$(EXECUTABLE) bytecodes/$@.bc 2> /dev/null