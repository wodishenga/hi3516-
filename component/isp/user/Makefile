#Create by 2016/10/20

#include Makefile.param in mpp
ifeq ($(PARAM_FILE), )
	PARAM_FILE:=../../../Makefile.param
	include $(PARAM_FILE)
endif


.PHONY:clean all firmware sensor

all: firmware sensor

firmware:
	@cd firmware && make
sensor:
	@cd sensor/$(HIARCH) && make
	
clean:
	@cd firmware && make clean
	@cd sensor/$(HIARCH) && make clean
