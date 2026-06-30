CLANG ?= clang
TARGET_TRIPLE ?= x86_64-elf
CFLAGS_COMMON := -std=c17 -Wall -Wextra -Werror -O2
HOST_BIN := m16_host_test
FREESTANDING_OBJ := m16_mcsfs_journal.o

.PHONY: all host freestanding audit clean
all: host freestanding audit

host: $(HOST_BIN)
	./$(HOST_BIN)

$(HOST_BIN): m16_mcsfs_journal.c
	$(CLANG) $(CFLAGS_COMMON) -DMCSOS_M16_HOST_TEST $< -o $@

freestanding: $(FREESTANDING_OBJ)

$(FREESTANDING_OBJ): m16_mcsfs_journal.c
	$(CLANG) $(CFLAGS_COMMON) -ffreestanding -fno-builtin -fno-stack-protector -fno-pic -mno-red-zone -target $(TARGET_TRIPLE) -c $< -o $@

audit: $(FREESTANDING_OBJ)
	nm -u $(FREESTANDING_OBJ) > nm_undefined.txt
	readelf -h $(FREESTANDING_OBJ) > readelf_header.txt
	objdump -dr $(FREESTANDING_OBJ) > objdump_disasm.txt
	sha256sum $(FREESTANDING_OBJ) > sha256sum.txt
	test ! -s nm_undefined.txt
	grep -q 'ELF64' readelf_header.txt
	grep -q 'Advanced Micro Devices X86-64' readelf_header.txt

clean:
	rm -f $(HOST_BIN) $(FREESTANDING_OBJ) nm_undefined.txt readelf_header.txt objdump_disasm.txt sha256sum.txt
