.PHONY: meta check smoke qemu-version clean distclean tree

BUILD_DIR := build
SMOKE_DIR := smoke

meta:
	@bash tools/check_env.sh

check:
	@bash tools/check_env.sh
	@shellcheck tools/check_env.sh

smoke:
	@mkdir -p $(BUILD_DIR)/smoke
	clang --target=x86_64-unknown-none \
		-ffreestanding \
		-fno-stack-protector \
		-fno-pic \
		-mno-red-zone \
		-mno-mmx -mno-sse -mno-sse2 \
		-Wall -Wextra -Werror \
		-std=c17 \
		-c $(SMOKE_DIR)/freestanding.c \
		-o $(BUILD_DIR)/smoke/freestanding.o
	readelf -h $(BUILD_DIR)/smoke/freestanding.o | tee $(BUILD_DIR)/smoke/readelf-header.txt
	objdump -drwC $(BUILD_DIR)/smoke/freestanding.o | tee $(BUILD_DIR)/smoke/objdump.txt >/dev/null
	file $(BUILD_DIR)/smoke/freestanding.o | tee $(BUILD_DIR)/smoke/file.txt

qemu-version:
	@qemu-system-x86_64 --version
	@echo "QEMU exists. M0 does not boot a kernel image."

tree:
	@tree -a -L 3

clean:
	rm -rf $(BUILD_DIR)/smoke

distclean:
	rm -rf $(BUILD_DIR)
