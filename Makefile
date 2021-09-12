# 主入口
include config.mk
all:

	@for dir in $(BUILD_DIR); \
	do \
		make -C $$dir; \
	done

clean:
	rm -rf app/link_obj app/dep hpServer
	rm -rf signal/*.gch app/*.gch

