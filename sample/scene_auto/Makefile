include ../Makefile.param

INC_FLAGS :=-I./include
INC_FLAGS +=-I./../scene
INC_FLAGS +=-I./tools/configaccess/include
INC_FLAGS +=-I./tools/iniparser/include
INC_FLAGS +=-I./../common
INC_FLAGS +=-I$(REL_DIR)/include

SRCS := $(wildcard ./tools/configaccess/src/*.c)
SRCS += $(wildcard ./tools/iniparser/src/*.c)
SRCS += $(wildcard ./src/core/*.c)
SRCS += $(wildcard ./src/sample/*.c)

.PHONY : clean all
# target source
TARGET := sample_scene
OBJS  := $(SRCS:%.c=%.o)
CFLAGS += $(INC_FLAGS)

all: $(TARGET)

$(TARGET):$(OBJS)
	@$(CC) $(CFLAGS) -lpthread -lm -o $@ $^ -Wl,--start-group $(MPI_LIBS) $(SENSOR_LIBS) $(AUDIO_LIBA) $(REL_LIB)/libsecurec.a -Wl,--end-group

clean:
	@rm -f $(TARGET)
	@rm -f $(OBJS)
