include ../Makefile.param

###########AMP SRCS & INC ############
AMP_CLIENT_SRCS := $(wildcard $(PWD)/$(ARM_ARCH)/client/*.c)
AMP_CLIENT_SRCS += $(wildcard $(PWD)/*.c)
AMP_SERVER_SRCS := $(wildcard $(PWD)/$(ARM_ARCH)/server/*.c)
AMP_INC += -I$(PWD)/
AMP_INC += -I$(PWD)/$(ARM_ARCH)/include

###########SMP SRCS & INC ############
SMP_SRCS := $(wildcard $(PWD)/$(ARM_ARCH)/*.c)
SMP_SRCS += $(wildcard $(PWD)/*.c)
SMP_INC := -I$(PWD)/

ifeq ($(ARM_ARCH), amp)

ifeq ($(OSTYPE), liteos)
TARGET := sample_vio_server
else
TARGET := sample_vio
endif

else
TARGET := sample_vio
endif

TARGET_PATH := $(PWD)/$(ARM_ARCH)

include $(PWD)/../$(ARM_ARCH)_$(OSTYPE).mak