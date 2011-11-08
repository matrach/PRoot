ROOTFS = ./rootfs
PROOT  = ../src/proot
CC     = gcc

TESTS_SH  = $(wildcard test-*.sh)
TESTS_C   = $(wildcard test-*.c)
TESTS_ALL = $(TESTS_SH) $(TESTS_C)

test check: $(TESTS_ALL)

.PHONY: setup test-%.sh test-%.c

test-%.sh: setup
	$($(quiet)TEST)
	$(Q)env PROOT=$(PROOT) ROOTFS=$(ROOTFS) sh -e $@ $(silently)

.SECONDARY: $(patsubst %.c,,$(TESTS_C))
test-%.c: setup $(ROOTFS)/bin/test-%
	$($(quiet)TEST)
	$(Q)$(PROOT) -b /proc $(ROOTFS) /bin/test-$* $(silently)

######################################################################
# Build a clean rootfs

setup: $(ROOTFS)/bin/true $(ROOTFS)/bin/false    \
       $(ROOTFS)/bin/pwd  $(ROOTFS)/bin/readlink

$(ROOTFS)/bin/%: # %.c
	@test -e $(dir $@) || mkdir -p $(dir $@)
	$(Q)$(CC) -static $*.c -o $@

######################################################################
# Beautified output

quiet_TEST = @echo "  TEST	$*"; $(TEST)

V = 0
ifeq ($(V), 0)
    quiet = quiet_
    Q     = @
    silently = >/dev/null 2>&1
else
    quiet = 
    Q     = 
    silently = 
endif