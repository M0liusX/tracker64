SRC_DIR := .
OBJ_DIR := build
SRC_FILES := $(SRC_DIR)/rsp.cpp
OBJ_FILES := $(OBJ_DIR)/rsp.o sljit/bin/sljitLir.o sljit/bin/regexJIT.o $(OBJ_DIR)/fixed-allocator.o $(OBJ_DIR)/main.o #$(OBJ_DIR)/memory.o
# LDFLAGS :=
CPPFLAGS := -g -fpermissive -std=c++17
CXXFLAGS := -I .

-include $(OBJ_FILES:.o=.d)

rsp.exe: $(OBJ_FILES)
	g++ -o $@ $^

$(OBJ_DIR)/fixed-allocator.o: ares/memory/fixed-allocator.cpp
	g++ $(CPPFLAGS) $(CXXFLAGS) -c -o $@ $<

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp $(SRC_DIR)/rsp-interface.cpp
	g++ $(CPPFLAGS) $(CXXFLAGS) -c -o $@ $<

$(OBJ_DIR)/main.o: main.cpp
	g++ $(CPPFLAGS) $(CXXFLAGS) -c -o $@ $<

# $(OBJ_DIR)/memory.o: nall/memory.cpp
# 	g++ $(CPPFLAGS) $(CXXFLAGS) -c -o $@ $<

# $(OBJ_DIR)/rdram.o: rdram/rdram.cpp
# 	g++ $(CPPFLAGS) $(CXXFLAGS) -c -o $@ $<