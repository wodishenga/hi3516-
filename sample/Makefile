ifeq ($(PARAM_FILE), )
     PARAM_FILE:=../Makefile.param
     include $(PARAM_FILE)
endif

ifeq ($(ARM_ARCH), amp)

ifeq ($(OSTYPE), liteos)

COMMON_DIR ?= $(shell pwd)/common
AUDIO_ADP_DIR ?= $(shell pwd)/audio/adp

include $(shell pwd)/./Makefile.param

ifeq ($(LITEOS_BIN_SUPPORT_VI), y)
AMP_SERVER_SRCS += $(wildcard $(shell pwd)/vio/$(ARM_ARCH)/server/*.c)
AMP_INC += -I$(shell pwd)/vio/
AMP_INC += -I$(shell pwd)/vio/$(ARM_ARCH)/include
endif

TARGET := sample
TARGET_PATH := $(shell pwd)

include $(shell pwd)/./$(ARM_ARCH)_$(OSTYPE).mak

else

all:
	@cd vio;     make
clean:
	@cd vio;     make clean
endif

else

all:
	@cd audio;   make
	@cd awb_online_calibration; make
	@cd dis;     make
	@cd fisheye; make
	@cd hifb;    make
	@cd lsc_online_cali; make
	@cd region;    make
	@cd snap;    make
	@cd traffic_capture; make
	@cd tde;     make
	@cd vdec;    make
	@cd venc;    make
	@cd vgs;     make
	@cd vio;     make
	@cd svp;     make
	@cd scene_auto;make
	@cd uvc_app; make
clean:
	@cd audio;   make clean
	@cd awb_online_calibration; make clean
	@cd dis;     make clean
	@cd fisheye; make clean
	@cd hifb;    make clean
	@cd lsc_online_cali; make clean
	@cd region;  make clean
	@cd snap;    make clean
	@cd traffic_capture; make clean
	@cd tde;     make clean
	@cd vdec;    make clean
	@cd venc;    make clean
	@cd vgs;     make clean
	@cd vio;     make clean
	@cd svp;     make clean
	@cd scene_auto;make clean
	@cd uvc_app; make clean

endif
