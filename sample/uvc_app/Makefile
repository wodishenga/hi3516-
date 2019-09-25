# Hisilicon Hi35xx uvc camera sample Makefile

include ../Makefile.param

UAC_COMPILE := n
#UAC_COMPILE := y

ifeq ($(UAC_COMPILE), y)
CFLAGS += -DHI_UAC_COMPILE
endif

SRC_PATH := $(shell pwd)
# INC_FLAGS += -I$(SRC_PATH)/include
# CFLAGS += -Wall -g $(INC_FLAGS)

SMP_SRCS := $(SRC_PATH)/application.c
SMP_SRCS += $(SRC_PATH)/camera.c
SMP_SRCS += $(SRC_PATH)/config_svc.c
SMP_SRCS += $(SRC_PATH)/dictionary.c
SMP_SRCS += $(SRC_PATH)/frame_cache.c
SMP_SRCS += $(SRC_PATH)/histream.c
SMP_SRCS += $(SRC_PATH)/hiuvc.c
SMP_SRCS += $(SRC_PATH)/iniparser.c
SMP_SRCS += $(SRC_PATH)/log.c
SMP_SRCS += $(SRC_PATH)/sample_yuv.c
SMP_SRCS += $(SRC_PATH)/uvc_venc_glue.c
SMP_SRCS += $(SRC_PATH)/uvc-gadget.c

SMP_SRCS += $(SRC_PATH)/sample_comm_venc.c
SMP_SRCS += $(SRC_PATH)/sample_venc.c

SMP_SRCS += $(SRC_PATH)/hiaudio.c
SMP_SRCS += $(SRC_PATH)/hiuac.c
SMP_SRCS += $(SRC_PATH)/uac-gadget.c

ifeq ($(UAC_COMPILE), y)
SMP_SRCS += $(SRC_PATH)/sample_audio.c
endif

# SMP_SRCS := $(wildcard *.c)

ifeq ($(CONFIG_HI_TDE_SUPPORT), y)
MPI_LIBS += $(REL_LIB)/libtde.a
endif

# ALSA
ifeq ($(UAC_COMPILE), y)
AUDIO_ALSA_DIR ?= $(shell pwd)/alsa
COMM_INC += -I$(AUDIO_ALSA_DIR)/include
MPI_LIBS += -L$(AUDIO_ALSA_DIR)/lib/ -lasound
endif

TARGET := uvc_app

TARGET_PATH := $(PWD)

# # target source
# OBJS  := $(SRCS:%.c=%.o)

# .PHONY : clean all

# all: $(TARGET)


# $(TARGET):$(COMM_OBJ) $(OBJS)
#   @$(CC) $(CFLAGS) -lpthread -lm -o $@ $^ $(MPI_LIBS) $(AUDIO_LIBA) $(SENSOR_LIBS)

# clean:
#   @rm -f $(TARGET)
#   @rm -f $(OBJS)
#   @rm -f $(COMM_OBJ)

# cleanstream:
#   @rm -f *.h264
#   @rm -f *.jpg
#   @rm -f *.mjp
#   @rm -f *.mp4

# compile linux or HuaweiLite
include $(PWD)/../$(ARM_ARCH)_$(OSTYPE).mak
