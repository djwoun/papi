# ***NOTE*** The Environment Variable PAPI_ROCM_ROOT must be defined for 
# programs to compile correctly. one typical location is /opt/rocm, but
# contact your sysadmin if you cannot find it.

NAME = rocm_smi
include ../../Makefile_comp_tests.target

PAPI_ROCM_ROOT ?= /opt/rocm
HIP_PATH        := $(PAPI_ROCM_ROOT)
HIPCC           := $(HIP_PATH)/bin/hipcc

# use hipcc as both C and C++ compiler
CC   := $(HIPCC)
CXX  := $(HIPCC)

INCLUDE += -I$(PAPI_ROCM_ROOT)/include
INCLUDE += -I$(PAPI_ROCM_ROOT)/include/rocm_smi
INCLUDE += -I$(PAPI_ROCM_ROOT)/include/hip
INCLUDE += -I$(PAPI_ROCM_ROOT)/include/hsa
INCLUDE += -I$(PAPI_ROCM_ROOT)/include/rocprofiler
INCLUDE += -I$(PAPI_ROCM_ROOT)/include/rocblas

LDFLAGS = -ldl -g -L$(PAPI_ROCM_ROOT)/lib/rocblas -lrocblas

# compile rules
%.o: %.c
	@echo "INCLUDE=" $(INCLUDE)
	$(CC)  $(CFLAGS) $(OPTFLAGS) $(INCLUDE) -c -o $@ $<

%.o: %.cpp
	@echo "INCLUDE=" $(INCLUDE)
	@echo "CFLAGS=" $(CFLAGS)
	$(CXX) $(CFLAGS) $(OPTFLAGS) $(INCLUDE) -c -o $@ $<

TESTS = rocm_command_line rocm_smi_all power_monitor_rocm \
        rocmsmi_example rocm_smi_writeTests gemm

rocm_smi_tests: $(TESTS)
 
gemm: gemm.o $(UTILOBJS) $(PAPILIB)
	$(HIPCC) $(CFLAGS) $(INCLUDE) -o $@ $< $(UTILOBJS) $(PAPILIB) $(LDFLAGS) -lpthread

clean:
	rm -f $(TESTS) *.o

checkpath: 
	echo PAPI_ROCM_ROOT = $(PAPI_ROCM_ROOT)
	echo HIP_PATH        = $(HIP_PATH)
	echo HIPCC           = $(HIPCC)
	echo INCLUDE         = $(INCLUDE)
	echo LDFLAGS         = $(LDFLAGS)
	echo CFLAGS          = $(CFLAGS)
