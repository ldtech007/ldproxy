TYPE = app

makefile_name := $(lastword $(MAKEFILE_LIST))
makefile_path := $(abspath $(makefile_name))
makefile_dir := $(patsubst %/,%,$(dir $(makefile_path)))

this_app_dir := $(makefile_dir)
this_app := $(notdir $(this_app_dir))

TARGET := h3_test
COMM_CODE := ${__LD_TECH_CODE_DIR}/source/comm_code/h3/request_header.c ${__LD_TECH_CODE_DIR}/source/comm_code/ldlog.c
INSTALL_DIR :=
COMM_SO :=  
private_comp_INC := -I${__LD_TECH_CODE_DIR}/source/comm_code -I${__LD_TECH_CODE_DIR}/source/comm_code/h3

private_ld_LINK := 

private_comp_flag := -g
private_comp_flag_c :=
private_comp_flag_cpp :=
private_ld_flag :=

obj_extra :=
private_install_cmd := 
private_make_release_O2 := no

include $(BUILD_SYS_DIR)/Makefile.$(TYPE)