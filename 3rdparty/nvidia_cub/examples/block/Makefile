#/******************************************************************************
# * Copyright (c) 2011, Duane Merrill.  All rights reserved.
# * Copyright (c) 2011-2018, NVIDIA CORPORATION.  All rights reserved.
# * 
# * Redistribution and use in source and binary forms, with or without
# * modification, are permitted provided that the following conditions are met:
# *	 * Redistributions of source code must retain the above copyright
# *	   notice, this list of conditions and the following disclaimer.
# *	 * Redistributions in binary form must reproduce the above copyright
# *	   notice, this list of conditions and the following disclaimer in the
# *	   documentation and/or other materials provided with the distribution.
# *	 * Neither the name of the NVIDIA CORPORATION nor the
# *	   names of its contributors may be used to endorse or promote products
# *	   derived from this software without specific prior written permission.
# * 
# * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
# * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# * DISCLAIMED. IN NO EVENT SHALL NVIDIA CORPORATION BE LIABLE FOR ANY
# * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
# *
#******************************************************************************/

#-------------------------------------------------------------------------------
#
# Makefile usage
#
# make <target> [sm=<XXX,...>] [cdp=<0|1>] [force32=<0|1>] [abi=<0|1>] [open64=<0|1>] [verbose=<0|1>] [keep=<0|1>]
#
#-------------------------------------------------------------------------------
 
include ../../common.mk 
 
 
#-------------------------------------------------------------------------------
# Includes
#-------------------------------------------------------------------------------

INC += -I$(CUB_DIR) -I$(CUB_DIR)test 



#-------------------------------------------------------------------------------
# Dependency Lists
#-------------------------------------------------------------------------------

rwildcard=$(foreach d,$(wildcard $1*),$(call rwildcard,$d/,$2) $(filter $(subst *,%,$2),$d))

DEPS =				$(CUB_DEPS) \
					$(CUB_DIR)test/Makefile \
					$(CUB_DIR)test/test_util.h \
					$(CUB_DIR)test/mersenne.h \
		
ALL = 	example_block_radix_sort \
	 	example_block_reduce \
	 	example_block_scan
		


#-------------------------------------------------------------------------------
# make default
#-------------------------------------------------------------------------------

default:


#-------------------------------------------------------------------------------
# make clean
#-------------------------------------------------------------------------------

clean :
	rm -f bin/*$(CPU_ARCH_SUFFIX)* 
	rm -f *.i* *.cubin *.cu.c *.cudafe* *.fatbin.c *.ptx *.hash *.cu.cpp *.o


#-------------------------------------------------------------------------------
# make all
#-------------------------------------------------------------------------------

all : $(ALL)

#-------------------------------------------------------------------------------
# make run
#-------------------------------------------------------------------------------

run : 
	for i in $(ALL); do ./bin/$${i}_$(BIN_SUFFIX) --device=$(device) || exit 1; done




#-------------------------------------------------------------------------------
# make example_block_reduce
#-------------------------------------------------------------------------------

example_block_reduce: bin/example_block_reduce_$(BIN_SUFFIX)

bin/example_block_reduce_$(BIN_SUFFIX) : example_block_reduce.cu $(DEPS)
	mkdir -p bin
	$(NVCC) $(DEFINES) $(SM_TARGETS) -o bin/example_block_reduce_$(BIN_SUFFIX) example_block_reduce.cu $(NVCCFLAGS) $(CPU_ARCH) $(INC) $(LIBS) -O3


#-------------------------------------------------------------------------------
# make example_block_scan
#-------------------------------------------------------------------------------

example_block_scan: bin/example_block_scan_$(BIN_SUFFIX)

bin/example_block_scan_$(BIN_SUFFIX) : example_block_scan.cu $(DEPS)
	mkdir -p bin
	$(NVCC) $(DEFINES) $(SM_TARGETS) -o bin/example_block_scan_$(BIN_SUFFIX) example_block_scan.cu $(NVCCFLAGS) $(CPU_ARCH) $(INC) $(LIBS) -O3


#-------------------------------------------------------------------------------
# make example_block_radix_sort
#-------------------------------------------------------------------------------

example_block_radix_sort: bin/example_block_radix_sort_$(BIN_SUFFIX)

bin/example_block_radix_sort_$(BIN_SUFFIX) : example_block_radix_sort.cu $(DEPS)
	mkdir -p bin
	$(NVCC) $(DEFINES) $(SM_TARGETS) -o bin/example_block_radix_sort_$(BIN_SUFFIX) example_block_radix_sort.cu $(NVCCFLAGS) $(CPU_ARCH) $(INC) $(LIBS) -O3
	
