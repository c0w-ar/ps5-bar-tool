# 1. SDK Setup
ifdef PS5_PAYLOAD_SDK
    include $(PS5_PAYLOAD_SDK)/toolchain/prospero.mk
else
    $(error PS5_PAYLOAD_SDK is undefined)
endif

# 2. Paths and Variables
BIN_DIR := bin
# Use the variable to define the ELF path consistently
ELF     := $(BIN_DIR)/bar_tool.elf

CFLAGS  := -Werror -Iinclude
# Ensure any linker flags from prospero.mk are included
LDFLAGS := 

# 3. Source Files
SRCS    := source/main.c source/bar_srv.c

# --- Rules ---

all: $(ELF)

# Create the directory
$(BIN_DIR):
	mkdir -p $(BIN_DIR)

# Build the ELF
# We use $(BIN_DIR) as an order-only prerequisite
$(ELF): $(SRCS) | $(BIN_DIR)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(SRCS)

clean:
	rm -rf $(BIN_DIR)

.PHONY: all clean