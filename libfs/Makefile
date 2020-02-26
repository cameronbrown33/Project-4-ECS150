# Target library
lib := libfs.a
objs := fs.o disk.o

CC := gcc
AR := ar rcs
CFLAGS := -Wall -Wextra -Werror

ifneq ($(D),1)
CFLAGS += -O2
else
CFLAGS += -g
endif

ifneq ($(V),1)
Q = @
endif

all: $(lib)

# Dep tracking *must* be below the 'all' rule
deps := $(patsubst %.o,%.d,$(objs))
-include $(deps)
DEPFLAGS = -MMD -MF $(@:.o=.d)

libfs.a: $(objs)
	@echo "AR $@"
	$(Q)$(AR) $@ $^
%.o: %.c
	@echo "CC $@"
	$(Q)$(CC) $(CFLAGS) -c -o $@ $< $(DEPFLAGS)
clean:
	@echo "clean"
	$(Q)rm -f $(lib) $(objs) $(deps)