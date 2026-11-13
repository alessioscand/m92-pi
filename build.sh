#!/bin/bash
# m92-pi build: ./build.sh [4|3|all]
# Circle in circle-rpi4mc / circle-rpi3mc (the same copies neo-pi uses).
# ./build.sh qemu: a Pi 3 kernel for QEMU -M raspi3b (circle-qemumc), with the
# USB keyboard working as a pad (PROJECT_FAKEPAD); not for real hardware
set -e
ROOT="$(cd "$(dirname "$0")" && pwd)"
TARGETS="${1:-all}"
[ "$TARGETS" = "all" ] && TARGETS="4 3"

for P in $TARGETS; do
	B="$ROOT/build/rpi$P"
	# keep the core's objects between builds: only what changed is rebuilt
	mkdir -p "$B"
	find "$B" -maxdepth 1 -type f ! -name '*.core.o' ! -name '*.core.cpp' ! -name 'libm92.a' -delete
	# the core's objects do not track headers: a changed header rebuilds the whole core
	if [ -f "$B/libm92.a" ] && [ -n "$(find "$ROOT/core" "$ROOT/third_party/mame" "$ROOT/third_party/ymfm" "$ROOT/third_party/z80" \( -name '*.h' -o -name '*.hxx' -o -name '*.ipp' -o -name '*.inc' \) -newer "$B/libm92.a")" ]; then
		rm -f "$B"/*.core.o "$B/libm92.a"
	fi
	cp "$ROOT"/common/*.c "$ROOT"/common/*.cpp "$ROOT"/src/*.c "$ROOT"/src/*.cpp "$B"/
	cp "$ROOT"/third_party/miniz/miniz.c "$B"/
	cp "$ROOT"/third_party/snes_ntsc/snes_ntsc.c "$B"/ 2>/dev/null || true
	# the core, flattened: m92.cpp -> m92.core.cpp, nec/v25.cpp -> v25.core.cpp ...
	for f in core/irem_machine.cpp core/m92.cpp core/m92_video.cpp core/m92_sound.cpp core/m92_api.cpp \
		 core/m90.cpp core/m90_video.cpp core/m90_sound.cpp \
		 third_party/mame/irem_cpu.cpp third_party/mame/nec/nec.cpp \
		 third_party/mame/nec/v25.cpp third_party/mame/nec/v25sfr.cpp \
		 third_party/ymfm/ymfm_opm.cpp third_party/z80/z80.c; do
		n="$(basename "${f%.c*}").core.cpp"		# z80.c too: built as C++
		cmp -s "$ROOT/$f" "$B/$n" || cp -p "$ROOT/$f" "$B/$n"
	done
	CIRCLE="$ROOT/circle-rpi${P}mc"
	[ "$P" = "qemu" ] && CIRCLE="$ROOT/circle-qemumc"
	sed "s|@CIRCLEHOME@|$CIRCLE|; s|@SRC@|$ROOT/src|g; s|@COMMON@|$ROOT/common|g; s|@CORE@|$ROOT/core|g; s|@THIRD@|$ROOT/third_party|g; s|@MINIZ@|$ROOT/third_party/miniz|; s|@NTSC@|$ROOT/third_party/snes_ntsc|" \
		"$ROOT/src/Makefile.in" > "$B/Makefile"
	[ "$P" = "qemu" ] && echo "DEFINE += -DPROJECT_FAKEPAD $QEMU_DEFINES" >> "$B/Makefile"
	make -C "$B" -j16 libm92.a
	make -C "$B" -j16
done

# sanity check: the whole kernel (code + data + bss) must fit in KERNEL_MAX_SIZE
for P in $TARGETS; do
	ELF=$(ls "$ROOT"/build/rpi$P/*.elf)
	[ "$P" = "qemu" ] && continue
	END=$("$ROOT"/toolchain/xpack-aarch64-none-elf-gcc-14.2.1-1.1/bin/aarch64-none-elf-nm "$ELF" | awk '$3=="__bss_end"||$3=="_end"{print $1}' | sort | tail -1)
	LIMIT=$((0x80000 + 0x1000000))
	if [ $((16#$END)) -ge $LIMIT ]; then echo "ERROR: kernel rpi$P too large (ends at 0x$END)"; exit 1; fi
	echo "rpi$P: kernel ends at 0x$END (limit $(printf 0x%x $LIMIT)) OK"
done
