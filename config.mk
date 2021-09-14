
export BUILD_ROOT = $(shell pwd)

export INCLUDE_PATH = $(BUILD_ROOT)/include

# 构建目录
BUILD_DIR = $(BUILD_ROOT)/signal/ 	\
			$(BUILD_ROOT)/proc/ 	\
			$(BUILD_ROOT)/app/ 

export DEBUG = true

