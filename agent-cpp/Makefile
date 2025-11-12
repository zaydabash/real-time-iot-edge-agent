.PHONY: build clean run

BUILD_DIR = build

build:
	@mkdir -p $(BUILD_DIR)
	cd $(BUILD_DIR) && cmake .. && make

clean:
	rm -rf $(BUILD_DIR)

run: build
	cd $(BUILD_DIR) && ./agent

