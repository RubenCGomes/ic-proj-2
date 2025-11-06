# Default target
.DEFAULT_GOAL := build

# Variables
BUILD_DIR := build
BIN_DIR := bin

# Build the project
build:
	@echo "Building the project..."
	@if [ ! -d $(BUILD_DIR) ]; then mkdir $(BUILD_DIR); fi
	@cd $(BUILD_DIR) && cmake .. && make
	@echo "Build complete. Binaries are in the '$(BIN_DIR)' directory."

# Clean build artifacts
clean:
	@echo "Cleaning build artifacts..."
	@rm -rf $(BUILD_DIR) $(BIN_DIR)
	@echo "Clean complete."

.PHONY: build clean
