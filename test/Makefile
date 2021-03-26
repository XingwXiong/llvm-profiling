PROJECT=test

COMMON_FLAGS=-g -O0 -std=c++1y -I.. -fexceptions  -fcxx-exceptions \
	-I `llvm-config --includedir --ldflags --system-libs --libs`
SRC_DIR?=$(PWD)
SRC=test.cpp

.PHONY:
	clean $(PROJECT)

test:
	@echo Compiling $(SRC)
	clang++ $(SRC) $(COMMON_FLAGS) -o $(PROJECT)

test.ll:
	@clang++ -S -emit-llvm $(SRC) -o test.ll

test_ins_reuse_dist: test.ll
	cd ../instruction_reuse_distance && make && cd -
	../instruction_reuse_distance/inst_reuse_dist test.ll $@.bc
	clang++ $@.bc ../runtime.cpp -o $@ $(COMMON_FLAGS)

test_unordered_map: test_unordered_map.cpp
	clang++ test_unordered_map.cpp -o test_unordered_map -I ..

test_lru_cache: test_lru_cache.cpp
	clang++ test_lru_cache.cpp -o test_lru_cache -I ..

clean:
	rm -rf $(PROJECT)

