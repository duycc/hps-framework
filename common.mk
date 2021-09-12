# 通用编译规则

ifeq ($(DEBUG),true)
CC = g++ -g
VERSION = debug
else
CC = g++
VERSION = release
endif

SRCS = $(wildcard *.cpp)
OBJS = $(SRCS:.cpp=.o)

DEPS = $(SRCS:.cpp=.d)

BIN := $(addprefix $(BUILD_ROOT)/,$(BIN))

LINK_OBJ_DIR = $(BUILD_ROOT)/app/link_obj
DEP_DIR = $(BUILD_ROOT)/app/dep

$(shell mkdir -p $(LINK_OBJ_DIR))
$(shell mkdir -p $(DEP_DIR))

# := 在解析阶段直接赋值常量字符串 立即展开
# = 在运行阶段，实际使用变量时再进行求值 延迟展开
OBJS := $(addprefix $(LINK_OBJ_DIR)/,$(OBJS))
DEPS := $(addprefix $(DEP_DIR)/,$(DEPS))

LINK_OBJ = $(wildcard $(LINK_OBJ_DIR)/*.o)
LINK_OBJ += $(OBJS)

all:$(DEPS) $(OBJS) $(BIN)

ifneq ("$(wildcard $(DEPS))","")  
include $(DEPS)  
endif

$(BIN):$(LINK_OBJ)
	@echo "------------------------build $(VERSION) mode--------------------------------!!!"
	$(CC) -o $@ $^

$(LINK_OBJ_DIR)/%.o:%.cpp
	$(CC) -I$(INCLUDE_PATH) -o $@ -c $(filter %.cpp,$^)

$(DEP_DIR)/%.d:%.cpp
	echo -n $(LINK_OBJ_DIR)/ > $@
	g++ -I$(INCLUDE_PATH) -MM $^ >> $@





