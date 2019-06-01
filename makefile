###############################################################################
## Simulator Makefile
###############################################################################

# Target
TARGET	   ?= armv6m-sim

# Options
CFLAGS	    = -O2 -fPIC
CFLAGS     += -Wno-write-strings

LDFLAGS     = 
LIBS        = -lelf -lbfd

# Source Files
SRC_DIR    = .

###############################################################################
# Variables
###############################################################################
OBJ_DIR      ?= obj/$(TARGET)/

###############################################################################
# Variables: Lists of objects, source and deps
###############################################################################
# SRC / Object list
src2obj       = $(OBJ_DIR)$(patsubst %$(suffix $(1)),%.o,$(notdir $(1)))

SRC          ?= $(foreach src,$(SRC_DIR),$(wildcard $(src)/*.cpp)) $(foreach src,$(SRC_DIR),$(wildcard $(src)/*.c))
OBJ          ?= $(foreach src,$(SRC),$(call src2obj,$(src)))

###############################################################################
# Rules: Compilation macro
###############################################################################
define template_cpp
$(call src2obj,$(1)): $(1) | $(OBJ_DIR)
	@echo "# Compiling $(notdir $(1))"
	@g++ $(CFLAGS) -c $$< -o $$@
endef

###############################################################################
# Rules
###############################################################################
all: $(TARGET)
	
$(OBJ_DIR):
	@mkdir -p $@

$(foreach src,$(SRC),$(eval $(call template_cpp,$(src))))	

$(TARGET): $(OBJ) makefile
	g++ $(LDFLAGS) $(OBJ) $(LIBS) -o $@

clean:
	-rm -rf $(OBJ_DIR) $(TARGET)
