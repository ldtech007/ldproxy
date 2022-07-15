ifneq ($(strip $(LD_TECH_ENV_MAKE_ALL_ISSET)), yes)
    $(error source env_make_all first)
endif

all: ld_tech_build

clean: ld_tech_clean


ld_tech_build:
	@echo -e "\n\n\n\n==== ld_tech_build start ===="
	cd source && make
	@echo -e "==== ld_tech_build finish ===="

ld_tech_clean:
	@echo -e "\n\n\n\n==== ld_tech_clean start ===="
	cd source && make clean
	@echo -e "==== ld_tech_clean finish ===="

package: 
	@echo -e "\n\n\n\n==== make package start ===="

	@echo -e "==== make package finish ===="

