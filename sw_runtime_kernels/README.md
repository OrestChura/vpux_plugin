# Software kernels for VPUX

### Components
- [`kernels`](kernels) - source code of (SHAVE) cpp kernels  
- [`jtag_tests`](jtag_tests) - testing system for developing, optimization, debug and validation of kernels on board or symulator through JTAG  
- [firmware.vpu.iot] repository is necessary (should be available locally)  

to build/execute the tests and to compile the kernels for VPUX compiler.
[`firmware_vpu_revision.txt`](firmware_vpu_revision.txt) - file must contain:
- corresponding branch and/or commit hash of firmware.vpu.iot repo to work
- MV_TOOLS_VERSION which should be used to build act_shave code binaries in the following form
```
mv_tools_version:   <version, for example 22.09.1-internal>
```
### Build/execute JTAG tests
#### Prerequisites

Create `FIRMWARE_VPU_DIR` environment variable.
```
export FIRMWARE_VPU_DIR=<absolute path to firmware.vpu.client repo>
```
(submodules presilicon and schema should be updated)

#### Build/execute the tests
build/execute for VPU3720 simulator:  
in `sw_runtime_kernels/jtag_tests/app/layer_tests/test_icv/build` run:  
`make -j8 all CONFIG_FILE=.config_sim_3720xx` to build  
`make start_simulator CONFIG_FILE=.config_sim_3720xx srvPort=30002` to start VPU3720 debug simulator  
`make run CONFIG_FILE=.config_sim_3720xx srvIP=127.0.0.1 srvPort=30002` to run tests

### Kernel's arrangements
ActShave sw kernel is represented in the VPUNN network (blob)
by fragments of precompiled elf file of the kernel (so called text segment and data segment).  
Text and data segments of the kernels are delivered together with VPUX network compiler
as separate binary files.  
The files are prepared by build procedure which can be done using [cmake script](kernels/CMakeLists.txt)

#### Compile/link the kernels to be added by VPUX compiler into the blob  
* See [Known issues](#known-issues)  

To prepare binaries the following steps should be done:
- create a temporary 'build' directory, cd into it
- call of cmake <path-to-kernels-dir> [options]
- call make

For detailed description of cmake options and corresponding examples, see [separate readme file](kernels/README.md)

The files are located in [`sw_runtime_kernels/kernels/prebuild`](kernels/prebuild) directory
and have names `sk.<entry point>.<platform>.<text or data as extension>.xdat`.
In each file the array filled by corresponding kernel segment and its size are defined in c/c++ syntax.
The array and size have the names
```
  unsigned char sk_<entry point>_<platform>_<text or data as extension>[] = { <hex values> };
  unsigned int sk_<entry point>_<platform>_<text or data as extension>_len = <len of array>;
```

#### Kernel creating/porting 
The main way of SW ActShave kernel execution is: one particular shave gets 
independent portion of input data located in NN CMX and returns the results 
into independent portion of output located in NN CMX.  
The kernel gets all input parameters as a one void pointer represented as uint32_t value  
example: [`void singleShaveSoftmax(uint32_t lParams)`](kernels/single_shave_softmax.cpp#L402)  
The content of the parameters pointed by [`lParams`](kernels/inc/param_softmax.h#L17) is a [contract between
vpux-compiler and particular sw kernel implementation]
The parameter structure can be any, but usually it includes:
- one or several input tensor descriptions,
- one or several output tensor descriptions,
- kernel specific parameters.  

In turn, tensor descriptions are represented as [MemRefData structure](kernels/inc/common_types.h#L78).  
Tensor data, dims and strides are pointed by dataAddr, dismAddr and stridesAddr correspondingly.
Kernel code can operate the `addr` fields as normal memory pointers,
which possibility is provided by windows-based virtual addressing feature of platform.  
Tensor dims and strides are written in 'memory' order
(dims[0] contains 'inner'dimension, dims[ndims - 1] contains 'outer' dimension).  
Strides are measured in bits.

To create SW kernel it is necessary:
- add kernel code in special source file in `sw_runtime_kernels` directory ([example](kernels/sigmoid_fp16.c))
  - the main kernel function (entry point) should be declared as `extern "C"`
  - if the kernel contains platform specific code
  the source file should be placed into platform specific  [subdirectory (3720)](kernels/3720) 
- add kernel parameters structure declaration in `sw_runtime_kernels/inc` directory ([example](kernels/inc/param_sigmoid.h))
which will be shared between vpux-compiler/vpux-plugin build and ActShave code moviCompile build
- Add script to prepare kernel binaries to be serialized into compiled network blob in `sw_runtime_kernels/kernels/prebuild`
([example and template](kernels/prebuild/singleShaveSoftmax.3010xx.sh)).
The files are prepared by special scripts in `sw_runtime_kernels/kernels/prebuild`
([example for softmax SW kernel](kernels/prebuild/singleShaveSoftmax.3010xx.sh))
- make the necessary preparations in vpux-compiler to provide
parsing, compilation and serialization through the compiler's dialects
- Add single layer vpux-compiler/vpux-plugin test.
- Add low-level JTAG tests as described in the next section. 

#### JTAG test creating/porting 
Low level sw-kernel JTAG testing system in vpux-plugin is being developed on the base of
[ICV tests in firmware.vpu.client repo for UPA shave SW layers].
Vpux-plugin JTAG testing system uses some elements from firmware.vpu.iot repo
including building system.
Low-level vpux-plugin JTAG tests builds and executes for VPUX37XX platform (moviSim and hardware).
Kernels execute on ActShave processors (VPUX37XX).
The tests are based on CustomCPP test family.
The kernel test:
- is executed on VPU leon processor,
- is represented by the test class derived from [`CustomCppTests` template class](jtag_tests/app/layer_tests/test_icv/leon/tests/custom_cpp_tests.h#L30)
([example](jtag_tests/app/layer_tests/test_icv/leon/tests/custom_cpp_sigmoid.cpp#L22)),
- prepares an instance of parameter structure
declared for the kernel in `kernels/inc` directory,
- provides the pointer to kernel entry point function represented in the array prepared by
`xx` linux utility and included in the test c++ module, for example:  
```
...
__attribute__((aligned(1024)))
#include "sk.hswish_fp16.3010xx.text.xdat"
...

```
- calculates the reference values and compare them with the values given by the tested kernel 
(or import precalculated values from a file),  
To create the test: 
- create special test source in `sw_runtime_kernels/jtag_tests/app/layer_tests/test_icv/leon/tests`,
for example, as copy of [sigmoid test](jtag_tests/app/layer_tests/test_icv/leon/tests/custom_cpp_sigmoid.cpp).
- Declare there the test class inside a unique test namespace ([example](jtag_tests/app/layer_tests/test_icv/leon/tests/custom_cpp_sigmoid.cpp#L16)).
- Override necessary class methods (initData, generateInputData, checkResults and others)
to prepare and provide kernel parameter structure and kernel entry point address,
generate inputs, reference outputs and compare results.
- Define in the kernel parameters structure header file 
the function to wrap kernel parameters structure into special common communication structure `BaseKernelParams`
([example](kernels/inc/param_sigmoid.h#L20)).
- Add entry point symbols into [svuSLKernels_EP.h](jtag_tests/app/nn/shave_lib/inc/layers/svuSLKernels_EP.h#L142) and
[jtag_tests/app/nn/shave_lib/shave/subdir.mk](jtag_tests/app/nn/shave_lib/shave/subdir.mk#L36) for KMB

### Known issues
- [\[MOVICOMPILER\] Some code sequences are not compiled for 3720 act shave with O3](E#26562) - 
in particular mvSubsoaces.cpp is not compiled for 3720 act shave with -O3
