#
# SPDX-FileCopyrightText: 2025 Zeal 8-bit Computer <contact@zeal8bit.com>
#
# SPDX-License-Identifier: Apache-2.0
#
COMMON_SRCS=src/main.c src/popup.c src/disk.c

CC=gcc
CFLAGS=-O2 -g -Wall -Iinclude -Iraylib/linux/include -Lraylib/linux/lib
LDFLAGS=-lraylib -lm
TARGET=zeal_disk_tool.elf
# Path for linuxdeploy
LINUXDEPLOY?=./linuxdeploy-x86_64.AppImage

all: $(TARGET)

##########################
# Build the Linux binary #
##########################
$(TARGET): src/disk_linux.c $(COMMON_SRCS) build/raylib-nuklear-linux.o
	$(CC) $(CFLAGS) -o $@ $^  $(LDFLAGS)

# To speed up the recompilation of the linux binary, make sure raylib-nuklear is already as an object file
build/raylib-nuklear-linux.o: src/raylib-nuklear.c
	mkdir -p build
	$(CC) $(CFLAGS) -c -o $@ $^


# Create an AppImage for the linux binary
deploy: $(TARGET)
	cp $< appdir/zeal_disk_tool
	$(LINUXDEPLOY) --appdir AppDeploy --executable=appdir/zeal_disk_tool --desktop-file appdir/zeal-disk-tool.desktop --icon-file appdir/zeal-disk-tool.png --output appimage

# Same for 32-bit #
$(TARGET)32: src/disk_linux.c $(COMMON_SRCS) build/raylib-nuklear-linux32.o
	$(CC) -m32 -Lraylib/linux32/lib $(CFLAGS) -o $@ $^ $(LDFLAGS)

build/raylib-nuklear-linux32.o: src/raylib-nuklear.c
	mkdir -p build
	$(CC) -m32 $(CFLAGS) -c -o $@ $^

deploy32: $(TARGET)32
	cp $< appdir/zeal_disk_tool
	$(LINUXDEPLOY) --appdir AppDeploy --executable=appdir/zeal_disk_tool --desktop-file appdir/zeal-disk-tool.desktop --icon-file appdir/zeal-disk-tool.png --output appimage

############################
# Build the Windows binary #
############################
WIN_CC=i686-w64-mingw32-gcc
WIN_WINDRES=i686-w64-mingw32-windres
WIN_CFLAGS=-O2 -Wall -Iinclude -Iraylib/win32/include -Lraylib/win32/lib
WIN_LDFLAGS=-lraylib -lwinmm -lgdi32 -static -mwindows
WIN_TARGET=zeal_disk_tool.exe

$(WIN_TARGET): src/disk_win.c $(COMMON_SRCS) appdir/zeal-disk-tool.res build/raylib-nuklear-win.o
	$(WIN_CC) $(WIN_CFLAGS) -o $(WIN_TARGET) $^  $(WIN_LDFLAGS)

# Rule to create the icon that will be embedded the EXE file
appdir/zeal-disk-tool.res: appdir/zeal-disk-tool.rc appdir/zeal-disk-tool.manifest appdir/zeal-disk-tool.ico
	$(WIN_WINDRES) $< -O coff -o $@

appdir/zeal-disk-tool.ico:
	convert appdir/zeal-disk-tool_16.png appdir/zeal-disk-tool_32.png appdir/zeal-disk-tool_48.png appdir/zeal-disk-tool_256.png $@

build/raylib-nuklear-win.o: src/raylib-nuklear.c
	mkdir -p build
	$(WIN_CC) $(CFLAGS) -c -o $@ $^


clean:
	rm -f $(WIN_TARGET) $(TARGET) appdir/zeal-disk-tool.res
	rm -rf build/
