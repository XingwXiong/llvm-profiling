PROJECT=inst_reuse_dist


COMMON_FLAGS=-g -O3 -std=c++1y -I.. \
	-I `llvm-config --includedir --cxxflags --ldflags --system-libs --libs`
SRC_DIR?=$(PWD)
SRC=InstReuseDist.cpp

.PHONY:
	clean $(PROJECT)

inst_reuse_dist:
	@echo Compiling $(SRC)
	clang++ $(SRC) $(COMMON_FLAGS) -o $(PROJECT)

clean:
	rm -rf $(PROJECT)
