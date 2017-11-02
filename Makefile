.SUFFIXES:

PLATFORMS	:=	$(patsubst platform.%.make,%,$(shell ls platform.*.make))
.PHONY: please_specify_target clean $(PLATFORMS)

please_specify_target:
	$(info Supported platforms: $(PLATFORMS))
	$(error Please specify a target: `make PLATFORM=x` or `make x`)

$(PLATFORMS):
	@make -f Makefile PLATFORM=$@

ifeq ($(strip $(PLATFORM)),)
.DEFAULT_GOAL	:=	please_specify_target

clean:
	@rm -vrf obj/ out/
else
COMNFLAGS	:=	-g -O2 -fdiagnostics-color=always -D_GNU_SOURCE -Wall -Wextra -pedantic
CXXFLAGS	:=	$(COMNFLAGS) $(CXXFLAGS) -std=c++14 -fno-rtti -fno-exceptions -fno-use-cxa-atexit
CFLAGS		:=	$(COMNFLAGS) $(CFLAGS) -std=c11
undefine COMNFLAGS

CFILES		:=	blowfish.c ntrcard.c
CXXFILES	:=

include platform.$(PLATFORM).make

OBJFILES	:=	$(OBJFILES) $(patsubst %,obj/$(PLATFORM)/%.o,$(CFILES) $(CXXFILES))

obj/$(PLATFORM)/%.c.o: src/%.c
	@echo $^ =\> $@
	@mkdir -p $(dir $@)
	@$(CC) -MMD -MP -MF obj/$(PLATFORM)/$*.c.d $(CFLAGS) -c $< -o $@ $(ERROR_FILTER)

obj/$(PLATFORM)/%.cpp.o: src/%.cpp
	@echo $^ =\> $@
	@mkdir -p $(dir $@)
	@$(CXX) -MMD -MP -MF obj/$(PLATFORM)/$*.cpp.d $(CXXFLAGS) -c $< -o $@ $(ERROR_FILTER)

obj/$(PLATFORM)/%.bin.o: %.bin
	@echo $^ =\> $@
	@mkdir -p $(dir $@)
	@ld -r -b binary $^ -o $@

%.a:
	@echo $^ =\> $@
	@rm -f $@
	@mkdir -p $(dir $@)
	@$(AR) -rcs $@ $^

clean:
	@rm -vrf obj/$(PLATFORM) out/$(PLATFORM)

fordka: out/$(PLATFORM)/libncgc.a
	@echo $^ =\> $@
	@cp $^ $@

out/$(PLATFORM)/libncgc.a: $(OBJFILES)

-include $(patsubst %,obj/$(PLATFORM)/%.d,$(CFILES) $(CXXFILES))

ifeq ($(.DEFAULT_GOAL),please_specify_target)
.DEFAULT_GOAL	:=	out/$(PLATFORM)/libncgc.a
endif
endif