# Makefile
TREE_ROOT := $(shell pwd)
BASE_DIR:=$(TREE_ROOT)

# Project Directory Name
PROJECT_DIR:=$(shell basename $(CURDIR))

# Peoject Name, Name of current Directory
PROJECT_NAME ?= $(PROJECT_DIR)

# Target Name
TARGET_NAME ?= $(PROJECT_NAME)


# Executables
CC:=$(PREFIX)gcc
CPP:=$(PREFIX)g++
AR:=$(PREFIX)ar
OBJCOPY:=$(PREFIX)objcopy
OBJDUMP:=$(PREFIX)objdump
SIZE:=$(PREFIX)size
GDB:=$(PREFIX)gdb
REMOVE:=rm -rf
MAKE:=make -s

# Directory Structure
SRC_DIR?=src
OBJ_DIR?=obj
LIB_DIR?=lib
INC_DIR?=inc
BIN_DIR?=bin

# Supported Extension
C_EXT := c
H_EXT := h
CPP_EXT := cpp
ASM_EXT := s
OBJ_EXT := o
LIB_EXT := a


# If SRC dir is empty search in Current Directory
ifeq "$(wildcard $(SRC_DIR) )" ""
	# src does not exists or is empty use current directory
	SRC_DIR :=.
	C_FILES := $(wildcard $(SRC_DIR)/*.$(C_EXT))

	# C++ Langauge
	CPP_FILES := $(wildcard $(SRC_DIR)/*.$(CPP_EXT) )

	# Assembly Langauge
	ASM_FILES := $(wildcard $(SRC_DIR)/*.$(ASM_EXT) )

else
	# C Langauge
	C_FILES := $(shell find $(SRC_DIR) -name \*.$(C_EXT) )

	# C++ Langauge
	CPP_FILES := $(shell find $(SRC_DIR) -name \*.$(CPP_EXT) )

	# Assembly Langauge
	ASM_FILES := $(shell find $(SRC_DIR) -name \*.$(ASM_EXT) )

endif


# If INC dir is empty search in Current Directory
ifeq "$(wildcard $(INC_DIR) )" ""
	INC_DIRS +=.
else
	# Include dirs
	INC_DIRS += $(sort $(dir $(shell find $(INC_DIR) -name \*.$(H_EXT) )))
endif

INCLUDE_FLAG = $(addprefix -I,$(INC_DIRS))

GLOBAL_CFLAG += $(INCLUDE_FLAG)

# Object Files and dependency files
C_OBJFILES := $(patsubst $(SRC_DIR)/%.$(C_EXT),$(OBJ_DIR)/%.$(OBJ_EXT),$(C_FILES))
C_DEPEND_FILES := $(patsubst %.$(OBJ_EXT),%.$(DEPEND_EXT),$(C_OBJFILES))

CPP_OBJFILES := $(patsubst $(SRC_DIR)/%.$(CPP_EXT),$(OBJ_DIR)/%.$(OBJ_EXT),$(CPP_FILES))
CPP_DEPEND_FILES := $(patsubst %.$(OBJ_EXT),%.$(DEPEND_EXT),$(CPP_OBJFILES))

ASM_OBJFILES := $(patsubst $(SRC_DIR)/%.$(ASM_EXT),$(OBJ_DIR)/%.$(OBJ_EXT),$(ASM_FILES))
ASM_DEPEND_FILES := $(patsubst %.$(OBJ_EXT),%.$(DEPEND_EXT),$(ASM_OBJFILES))

# Objects
OBJECTS := $(C_OBJFILES) $(CPP_OBJFILES) $(ASM_OBJFILES)

# Depend Files
DEPEND_FILES := $(C_DEPEND_FILES) $(CPP_DEPEND_FILES) $(ASM_DEPEND_FILES)


# Add Custom libraries
LIBS+=

# Libraries
LIB_FLAG+= -pthread -lpthread

############################################
# Create obj Dirs
############################################
OBJ_DIRS:=$(dir $(OBJECTS))
ifneq  ("$(OBJ_DIRS)","")
    $(shell mkdir -p $(OBJ_DIRS))
endif

# If Verbose mode is not requsted then silent output
ifeq (,$(V))
.SILENT:
endif

# Optimization level
OPTIMIZE := -Os

#Disable logging
ifneq ("$(TRACE_LEVEL)","")
    GLOBAL_CFLAG+=-D_TRACE_LEVEL_=$(TRACE_LEVEL)
endif

GLOBAL_CFLAG += -Wall


CFLAGS := $(GLOBAL_CFLAG)

LDFLAGS := $(LIB_FLAG)

# Target Binary
TARGET ?= $(BIN_DIR)/$(TARGET_NAME)

all:$(TARGET)

# Executable file
$(BIN_DIR)/$(TARGET_NAME):  $(OBJECTS) $(LIB_DEPENDS)
	@echo Creating $(BIN_DIR)/$(TARGET_NAME)
	@mkdir -p $(BIN_DIR);
	$(CPP) $(OBJECTS) $(LDFLAGS) -o $@

#Compile C files
$(C_OBJFILES): $(OBJ_DIR)/%.$(OBJ_EXT): $(SRC_DIR)/%.$(C_EXT)
	@$(CC) -E $(CFLAGS) $< > $(OBJ_DIR)/$*.E
	$(CC) $(CFLAGS) -c $< -o $@
	@$(CC) -MM $(CFLAGS) $< > $(OBJ_DIR)/$*.d
	@mv -f $(OBJ_DIR)/$*.d $(OBJ_DIR)/$*.d.tmp;
	@sed -e 's|.*:|$(OBJ_DIR)/$*.o:|' < $(OBJ_DIR)/$*.d.tmp > $(OBJ_DIR)/$*.d
	@rm -f $(OBJ_DIR)/$*.d.tmp

# Compile cpp files
$(CPP_OBJFILES): $(OBJ_DIR)/%.$(OBJ_EXT): $(SRC_DIR)/%.$(CPP_EXT)
	@$(CPP) -E $(CPPFLAGS) $< > $(OBJ_DIR)/$*.E
	$(CPP) $(CPPFLAGS) -c $< -o $@
	@$(CPP) -MM $(CPPFLAGS) $< > $(OBJ_DIR)/$*.d
	@mv -f $(OBJ_DIR)/$*.d $(OBJ_DIR)/$*.d.tmp
	@sed -e 's|.*:|$(OBJ_DIR)/$*.o:|' < $(OBJ_DIR)/$*.d.tmp > $(OBJ_DIR)/$*.d
	@rm -f $(OBJ_DIR)/$*.d.tmp

# Compile assembly file
$(ASM_OBJFILES): $(OBJ_DIR)/%.$(OBJ_EXT): $(SRC_DIR)/%.$(ASM_EXT)
	@$(CPP) -E $(CPPFLAGS) $< > $(OBJ_DIR)/$*.E
	$(CPP) $(CPPFLAGS) -c $< -o $@
	@$(CPP) -MM $(CPPFLAGS) $< > $(OBJ_DIR)/$*.d
	@mv -f $(OBJ_DIR)/$*.d $(OBJ_DIR)/$*.d.tmp
	@sed -e 's|.*:|$(OBJ_DIR)/$*.o:|' < $(OBJ_DIR)/$*.d.tmp > $(OBJ_DIR)/$*.d
	@rm -f $(OBJ_DIR)/$*.d.tmp

# Library File
$(LIB_DIR)/$(LIB_PREFIX)$(LIBNAME).$(LIB_EXT) : $(OBJECTS)
	@mkdir -p $(LIB_DIR);
	$(AR) $(ARFLAGS) $@ $(OBJECTS)
	

# Clean Everything
clean cleanDebug cleanRelease:
	@echo "Remove binary and objects Files"
	@$(REMOVE) $(OBJ_DIR)
	@$(REMOVE) $(LIB_DIR)
	@$(REMOVE) $(BIN_DIR)

.PHONY: clean
	
