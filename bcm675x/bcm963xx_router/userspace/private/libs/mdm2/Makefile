LIB = libmdm2.so
default: dynamic


libmdm2.so:
	cp libmdm2.so_$(PROFILE).save libmdm2.so

dynamic: generic_private_lib_install

clean:
	rm -f libmdm2.so

CURR_DIR := $(shell pwd)
BUILD_DIR := $(subst /userspace, /userspace,$(CURR_DIR))
BUILD_DIR := $(word 1, $(BUILD_DIR))
include $(BUILD_DIR)/make.common
