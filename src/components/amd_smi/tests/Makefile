<<<<<<< HEAD
# ***NOTE*** The Environment Variable PAPI_AMDSMI_ROOT must be defined for
# programs to compile correctly. One typical location is /opt/rocm.
PAPI_AMDSMI_ROOT ?= /opt/rocm-6.4.0
HIP_PATH := $(PAPI_AMDSMI_ROOT)
HIPCC := $(HIP_PATH)/bin/hipcc

# Toolchain / flags
CFLAGS   ?= -DPAPI_NUM_COMP=3
CSTD     ?= -std=c11
HIPFLAGS ?= $(CFLAGS) -std=c++17
OPTFLAGS ?= -O2

UTILOBJS ?= ../../../testlib/libtestlib.a
PAPILIB  ?= ../../../libpapi.a

# Includes (from your logs)
BASE_INCLUDES_FROM_LOG ?= -I. -I../../.. -I../../../testlib -I../../../validation_tests
PAPI_DIR_INCLUDE ?= /home/dwoun/apps/papi/include

EFFECTIVE_INCLUDE := $(BASE_INCLUDES_FROM_LOG)
EFFECTIVE_INCLUDE += -I$(PAPI_DIR_INCLUDE)
EFFECTIVE_INCLUDE += -I$(PAPI_AMDSMI_ROOT)/include
EFFECTIVE_INCLUDE += -I$(PAPI_AMDSMI_ROOT)/include/rocm_smi
EFFECTIVE_INCLUDE += -I$(PAPI_AMDSMI_ROOT)/include/hip
EFFECTIVE_INCLUDE += -I$(PAPI_AMDSMI_ROOT)/include/hsa
EFFECTIVE_INCLUDE += -I$(PAPI_AMDSMI_ROOT)/include/rocprofiler
EFFECTIVE_INCLUDE += -I$(PAPI_AMDSMI_ROOT)/include/rocblas

# Linker flags
COMMON_LDFLAGS  = -ldl -g -lpthread
LDFLAGS_AMDSMI  = $(UTILOBJS) $(PAPILIB) -L$(PAPI_AMDSMI_ROOT)/lib -lamd_smi $(COMMON_LDFLAGS)
LDFLAGS_GEMM    = $(UTILOBJS) $(PAPILIB) -L$(PAPI_AMDSMI_ROOT)/lib -lrocblas $(COMMON_LDFLAGS)

# New AMD SMI tests
TESTS_AMDSMI := amdsmi_energy_monotonic amdsmi_ctx_conflict

# Default target (now builds amdsmi_hello)
all: amdsmi_hello amdsmi_basics amdsmi_gemm $(TESTS_AMDSMI)

# ------------------------
# Pattern rules
# ------------------------

# .c -> .o
%.o: %.c
	@echo "Compiling C source $< to $@..."
	@echo "HIPCC: $(HIPCC)"
	@echo "CFLAGS: $(CFLAGS) CSTD: $(CSTD) OPTFLAGS: $(OPTFLAGS)"
	@echo "INCLUDES: $(EFFECTIVE_INCLUDE)"
	$(HIPCC) $(CFLAGS) $(CSTD) $(OPTFLAGS) $(EFFECTIVE_INCLUDE) -c $< -o $@

# HIP override for the GEMM-like file (renamed): compile with -x hip
amdsmi_gemm.o: amdsmi_gemm.c
	@echo "Compiling HIP source $< to $@..."
	@echo "HIPCC: $(HIPCC)"
	@echo "HIPFLAGS: $(HIPFLAGS) OPTFLAGS: $(OPTFLAGS)"
	@echo "INCLUDES: $(EFFECTIVE_INCLUDE)"
	$(HIPCC) $(HIPFLAGS) $(OPTFLAGS) $(EFFECTIVE_INCLUDE) -x hip -c $< -o $@

# ------------------------
# Link rules
# ------------------------

# amdsmi_hello replaces amdsmi_example
amdsmi_hello: amdsmi_hello.o $(UTILOBJS) $(PAPILIB)
	@echo "Linking $@..."
	@echo "Using LDFLAGS_AMDSMI: $(LDFLAGS_AMDSMI)"
	$(HIPCC) $(CFLAGS) $(CSTD) $(OPTFLAGS) $(EFFECTIVE_INCLUDE) -o $@ amdsmi_hello.o $(LDFLAGS_AMDSMI)

# Back-compat alias: allow "make amdsmi_example" to build amdsmi_hello
amdsmi_example: amdsmi_hello
	@true

amdsmi_basics: amdsmi_basics.o $(UTILOBJS) $(PAPILIB)
	@echo "Linking $@..."
	@echo "Using LDFLAGS_AMDSMI: $(LDFLAGS_AMDSMI)"
	$(HIPCC) $(CFLAGS) $(CSTD) $(OPTFLAGS) $(EFFECTIVE_INCLUDE) -o $@ amdsmi_basics.o $(LDFLAGS_AMDSMI)

# renamed GEMM target
amdsmi_gemm: amdsmi_gemm.o $(UTILOBJS) $(PAPILIB)
	@echo "Linking $@..."
	@echo "Using LDFLAGS_GEMM: $(LDFLAGS_GEMM)"
	$(HIPCC) $(HIPFLAGS) $(OPTFLAGS) $(EFFECTIVE_INCLUDE) -o $@ amdsmi_gemm.o $(LDFLAGS_GEMM)

# New tests
amdsmi_energy_monotonic: amdsmi_energy_monotonic.o $(UTILOBJS) $(PAPILIB)
	@echo "Linking $@..."
	@echo "Using LDFLAGS_AMDSMI: $(LDFLAGS_AMDSMI)"
	$(HIPCC) $(CFLAGS) $(CSTD) $(OPTFLAGS) $(EFFECTIVE_INCLUDE) -o $@ amdsmi_energy_monotonic.o $(LDFLAGS_AMDSMI)

amdsmi_ctx_conflict: amdsmi_ctx_conflict.o $(UTILOBJS) $(PAPILIB)
	@echo "Linking $@..."
	@echo "Using LDFLAGS_AMDSMI: $(LDFLAGS_AMDSMI)"
	$(HIPCC) $(CFLAGS) $(CSTD) $(OPTFLAGS) $(EFFECTIVE_INCLUDE) -o $@ amdsmi_ctx_conflict.o $(LDFLAGS_AMDSMI)

$(UTILOBJS):
	$(MAKE) -C ../../../testlib libtestlib.a

# Convenience meta-target
tests: $(TESTS_AMDSMI)

# ------------------------
# Cleanup and diagnostics
# ------------------------

clean:
	@echo "Cleaning up..."
	rm -f \
	  amdsmi_hello amdsmi_hello.o \
	  amdsmi_basics amdsmi_basics.o \
	  amdsmi_gemm amdsmi_gemm.o \
	  gemm gemm.o \
	  amdsmi_energy_monotonic amdsmi_energy_monotonic.o \
	  amdsmi_ctx_conflict amdsmi_ctx_conflict.o \
	  *.exe

checkpath:
	@echo "--- Variables ---"
	@echo "PAPI_AMDSMI_ROOT = $(PAPI_AMDSMI_ROOT)"
	@echo "HIPCC = $(HIPCC)"
	@echo "INCLUDES = $(EFFECTIVE_INCLUDE)"
	@echo "CFLAGS = $(CFLAGS)"
	@echo "CSTD = $(CSTD)"
	@echo "HIPFLAGS = $(HIPFLAGS)"
	@echo "OPTFLAGS = $(OPTFLAGS)"
	@echo "UTILOBJS = $(UTILOBJS)"
	@echo "PAPILIB = $(PAPILIB)"
	@echo "COMMON_LDFLAGS = $(COMMON_LDFLAGS)"
	@echo "LDFLAGS_AMDSMI = $(LDFLAGS_AMDSMI)"
	@echo "LDFLAGS_GEMM = $(LDFLAGS_GEMM)"
	@echo "TESTS_AMDSMI = $(TESTS_AMDSMI)"

.PHONY: all clean checkpath tests \
        amdsmi_hello amdsmi_example amdsmi_basics amdsmi_gemm gemm \
        amdsmi_energy_monotonic amdsmi_ctx_conflict
=======
# ***NOTE*** The Environment Variable PAPI_AMDSMI_ROOT must be defined for
# programs to compile correctly. One typical location is /opt/rocm.
PAPI_AMDSMI_ROOT ?= /opt/rocm-6.4.0
HIP_PATH := $(PAPI_AMDSMI_ROOT)
HIPCC := $(HIP_PATH)/bin/hipcc

# Toolchain / flags
CFLAGS   ?= -DPAPI_NUM_COMP=3
CXXFLAGS ?= $(CFLAGS) -std=c++17
OPTFLAGS ?= -O2

UTILOBJS ?= ../../../testlib/libtestlib.a
PAPILIB  ?= ../../../libpapi.a

# Includes (from your logs)
BASE_INCLUDES_FROM_LOG ?= -I. -I../../.. -I../../../testlib -I../../../validation_tests
PAPI_DIR_INCLUDE ?= /home/dwoun/apps/papi/include

EFFECTIVE_INCLUDE := $(BASE_INCLUDES_FROM_LOG)
EFFECTIVE_INCLUDE += -I$(PAPI_DIR_INCLUDE)
EFFECTIVE_INCLUDE += -I$(PAPI_AMDSMI_ROOT)/include
EFFECTIVE_INCLUDE += -I$(PAPI_AMDSMI_ROOT)/include/amd_smi
EFFECTIVE_INCLUDE += -I$(PAPI_AMDSMI_ROOT)/include/hip
EFFECTIVE_INCLUDE += -I$(PAPI_AMDSMI_ROOT)/include/hsa
EFFECTIVE_INCLUDE += -I$(PAPI_AMDSMI_ROOT)/include/rocprofiler
EFFECTIVE_INCLUDE += -I$(PAPI_AMDSMI_ROOT)/include/rocblas

# Linker flags
COMMON_LDFLAGS  = -ldl -g -lpthread
LDFLAGS_AMDSMI  = $(UTILOBJS) $(PAPILIB) -L$(PAPI_AMDSMI_ROOT)/lib -lamd_smi $(COMMON_LDFLAGS)
LDFLAGS_GEMM    = $(UTILOBJS) $(PAPILIB) -L$(PAPI_AMDSMI_ROOT)/lib -lrocblas $(COMMON_LDFLAGS)

# New AMD SMI tests
TESTS_AMDSMI := amdsmi_energy_monotonic amdsmi_ctx_conflict

# Default target (now builds amdsmi_hello)
all: amdsmi_hello amdsmi_basics amdsmi_gemm $(TESTS_AMDSMI)

# ------------------------
# Pattern rules
# ------------------------

# .c -> .o
%.o: %.c
	@echo "Compiling C source $< to $@..."
	@echo "HIPCC: $(HIPCC)"
	@echo "CFLAGS: $(CFLAGS) OPTFLAGS: $(OPTFLAGS)"
	@echo "INCLUDES: $(EFFECTIVE_INCLUDE)"
	$(HIPCC) $(CFLAGS) $(OPTFLAGS) $(EFFECTIVE_INCLUDE) -c $< -o $@

# .cpp -> .o (generic C++)
%.o: %.cpp
	@echo "Compiling C++ source $< to $@..."
	@echo "HIPCC: $(HIPCC)"
	@echo "CXXFLAGS: $(CXXFLAGS) OPTFLAGS: $(OPTFLAGS)"
	@echo "INCLUDES: $(EFFECTIVE_INCLUDE)"
	$(HIPCC) $(CXXFLAGS) $(OPTFLAGS) $(EFFECTIVE_INCLUDE) -c $< -o $@

# HIP override for the GEMM-like file (renamed): compile with -x hip
amdsmi_gemm.o: amdsmi_gemm.cpp
	@echo "Compiling HIP source $< to $@..."
	@echo "HIPCC: $(HIPCC)"
	@echo "CXXFLAGS: $(CXXFLAGS) OPTFLAGS: $(OPTFLAGS)"
	@echo "INCLUDES: $(EFFECTIVE_INCLUDE)"
	$(HIPCC) $(CXXFLAGS) $(OPTFLAGS) $(EFFECTIVE_INCLUDE) -x hip -c $< -o $@

# ------------------------
# Link rules
# ------------------------

# amdsmi_hello replaces amdsmi_example
amdsmi_hello: amdsmi_hello.o $(UTILOBJS) $(PAPILIB)
	@echo "Linking $@..."
	@echo "Using LDFLAGS_AMDSMI: $(LDFLAGS_AMDSMI)"
	$(HIPCC) $(CXXFLAGS) $(OPTFLAGS) $(EFFECTIVE_INCLUDE) -o $@ amdsmi_hello.o $(LDFLAGS_AMDSMI)

# Back-compat alias: allow "make amdsmi_example" to build amdsmi_hello
amdsmi_example: amdsmi_hello
	@true

amdsmi_basics: amdsmi_basics.o $(UTILOBJS) $(PAPILIB)
	@echo "Linking $@..."
	@echo "Using LDFLAGS_AMDSMI: $(LDFLAGS_AMDSMI)"
	$(HIPCC) $(CXXFLAGS) $(OPTFLAGS) $(EFFECTIVE_INCLUDE) -o $@ amdsmi_basics.o $(LDFLAGS_AMDSMI)

# renamed GEMM target
amdsmi_gemm: amdsmi_gemm.o $(UTILOBJS) $(PAPILIB)
	@echo "Linking $@..."
	@echo "Using LDFLAGS_GEMM: $(LDFLAGS_GEMM)"
	$(HIPCC) $(CXXFLAGS) $(OPTFLAGS) $(EFFECTIVE_INCLUDE) -o $@ amdsmi_gemm.o $(LDFLAGS_GEMM)

# New tests
amdsmi_energy_monotonic: amdsmi_energy_monotonic.o $(UTILOBJS) $(PAPILIB)
	@echo "Linking $@..."
	@echo "Using LDFLAGS_AMDSMI: $(LDFLAGS_AMDSMI)"
	$(HIPCC) $(CXXFLAGS) $(OPTFLAGS) $(EFFECTIVE_INCLUDE) -o $@ amdsmi_energy_monotonic.o $(LDFLAGS_AMDSMI)

amdsmi_ctx_conflict: amdsmi_ctx_conflict.o $(UTILOBJS) $(PAPILIB)
	@echo "Linking $@..."
	@echo "Using LDFLAGS_AMDSMI: $(LDFLAGS_AMDSMI)"
	$(HIPCC) $(CXXFLAGS) $(OPTFLAGS) $(EFFECTIVE_INCLUDE) -o $@ amdsmi_ctx_conflict.o $(LDFLAGS_AMDSMI)

# Convenience meta-target
tests: $(TESTS_AMDSMI)

# ------------------------
# Cleanup and diagnostics
# ------------------------

clean:
	@echo "Cleaning up..."
	rm -f \
	  amdsmi_hello amdsmi_hello.o \
	  amdsmi_basics amdsmi_basics.o \
	  amdsmi_gemm amdsmi_gemm.o \
	  gemm gemm.o \
	  amdsmi_energy_monotonic amdsmi_energy_monotonic.o \
	  amdsmi_ctx_conflict amdsmi_ctx_conflict.o \
	  *.exe

checkpath:
	@echo "--- Variables ---"
	@echo "PAPI_AMDSMI_ROOT = $(PAPI_AMDSMI_ROOT)"
	@echo "HIPCC = $(HIPCC)"
	@echo "INCLUDES = $(EFFECTIVE_INCLUDE)"
	@echo "CFLAGS = $(CFLAGS)"
	@echo "CXXFLAGS = $(CXXFLAGS)"
	@echo "OPTFLAGS = $(OPTFLAGS)"
	@echo "UTILOBJS = $(UTILOBJS)"
	@echo "PAPILIB = $(PAPILIB)"
	@echo "COMMON_LDFLAGS = $(COMMON_LDFLAGS)"
	@echo "LDFLAGS_AMDSMI = $(LDFLAGS_AMDSMI)"
	@echo "LDFLAGS_GEMM = $(LDFLAGS_GEMM)"
	@echo "TESTS_AMDSMI = $(TESTS_AMDSMI)"

.PHONY: all clean checkpath tests \
        amdsmi_hello amdsmi_example amdsmi_basics amdsmi_gemm gemm \
        amdsmi_energy_monotonic amdsmi_ctx_conflict
>>>>>>> b37618394 (minor makefile change)
