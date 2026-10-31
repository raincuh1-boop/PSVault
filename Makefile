TITLE := PSVault
VERSION := 01.01
TITLE_ID := PSVT00001
CONTENT_ID := IV0000-PSVT00001_00-PSVAULT000000001
TOOLCHAIN := $(OO_PS4_TOOLCHAIN)
CORE_LIBS := -lc -lkernel -lc++ -lSceLibcInternal -lSceNet -lSceSsl -lSceHttp -lSceSysmodule -lScePad -lSceUserService -lSceVideoOut -lSDL2
ALL_STUBS := $(sort $(patsubst $(TOOLCHAIN)/lib/lib%.so,-l%,$(wildcard $(TOOLCHAIN)/lib/lib*.so)) $(patsubst $(TOOLCHAIN)/lib/lib%.a,-l%,$(wildcard $(TOOLCHAIN)/lib/lib*.a)))
EXTRA_LIBS := $(filter-out $(CORE_LIBS),$(ALL_STUBS))
SRCDIR := src
INTDIR := build
OUT_ELF := $(INTDIR)/psvault.elf
OUT_OELF := $(INTDIR)/psvault.oelf
CC := clang
LD := ld.lld
PKG := $(TOOLCHAIN)/bin/linux
CFILES := $(wildcard $(SRCDIR)/*.c)
OBJS := $(patsubst $(SRCDIR)/%.c,$(INTDIR)/%.o,$(CFILES))
CFLAGS := --target=x86_64-pc-freebsd12-elf -D__PS4__ -D__OPENORBIS__ -D__ORBIS__ -fPIC -funwind-tables -c -isysroot $(TOOLCHAIN) -isystem $(TOOLCHAIN)/include
LDFLAGS := -m elf_x86_64 -pie --script $(TOOLCHAIN)/link.x --eh-frame-hdr -L$(TOOLCHAIN)/lib $(CORE_LIBS) --as-needed $(EXTRA_LIBS) --no-as-needed $(TOOLCHAIN)/lib/crt1.o
ASSETS := $(shell find assets -type f 2>/dev/null | LC_ALL=C sort)
MODULES := $(shell find sce_module -type f 2>/dev/null | LC_ALL=C sort)

all: $(CONTENT_ID).pkg
$(INTDIR):
	mkdir -p $(INTDIR)
$(INTDIR)/%.o: $(SRCDIR)/%.c | $(INTDIR)
	$(CC) $(CFLAGS) -o $@ $<
eboot.bin: $(OBJS)
	$(LD) $(OBJS) -o $(OUT_ELF) $(LDFLAGS)
	$(PKG)/create-fself -in=$(OUT_ELF) -out=$(OUT_OELF) --eboot "eboot.bin" --paid 0x3100000000000001
sce_sys/param.sfo: Makefile
	$(PKG)/PkgTool.Core sfo_new $@
	$(PKG)/PkgTool.Core sfo_setentry $@ APP_TYPE --type Integer --maxsize 4 --value 1
	$(PKG)/PkgTool.Core sfo_setentry $@ APP_VER --type Utf8 --maxsize 8 --value '$(VERSION)'
	$(PKG)/PkgTool.Core sfo_setentry $@ ATTRIBUTE --type Integer --maxsize 4 --value 0
	$(PKG)/PkgTool.Core sfo_setentry $@ CATEGORY --type Utf8 --maxsize 4 --value 'gd'
	$(PKG)/PkgTool.Core sfo_setentry $@ CONTENT_ID --type Utf8 --maxsize 48 --value '$(CONTENT_ID)'
	$(PKG)/PkgTool.Core sfo_setentry $@ DOWNLOAD_DATA_SIZE --type Integer --maxsize 4 --value 0
	$(PKG)/PkgTool.Core sfo_setentry $@ SYSTEM_VER --type Integer --maxsize 4 --value 0
	$(PKG)/PkgTool.Core sfo_setentry $@ TITLE --type Utf8 --maxsize 128 --value '$(TITLE)'
	$(PKG)/PkgTool.Core sfo_setentry $@ TITLE_ID --type Utf8 --maxsize 12 --value '$(TITLE_ID)'
	$(PKG)/PkgTool.Core sfo_setentry $@ VERSION --type Utf8 --maxsize 8 --value '$(VERSION)'
ASSETS_AND_MODULES := $(ASSETS) $(MODULES)
pkg.gp4: eboot.bin sce_sys/param.sfo sce_sys/icon0.png sce_sys/about/right.sprx $(ASSETS_AND_MODULES)
	python3 tools/make_gp4.py --out $@ --content-id $(CONTENT_ID) $^
$(CONTENT_ID).pkg: pkg.gp4
	$(PKG)/PkgTool.Core pkg_build $< .
clean:
	rm -rf $(INTDIR) eboot.bin pkg.gp4 sce_sys/param.sfo *.pkg
.PHONY: all clean
