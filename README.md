# Embdebug target for CORE-V

Embdebug is the generic GDB/LLDB debug server for embedded systems.  This is the target specific library for embedded CORE-V targets.

These instructions only focus on building the target standalone.  However the more usual way to build the target is by being symbolically linked into the Embdebug source tree, and being built together with Embdebug.  This is documented as part of the Embdebug repository.

## Building the target

### Building the Verilator model

Check out [core-v-mcu](https://github.com/openhwgroup/core-v-mcu) and switch
to its top level directory. Build the model library
```
make model-lib
```

**Note.** You need to have FuseSoc and Verilator (4.20 or greater) installed.

Make a note of the directory holding the generated model, which will be something like.
```
build/openhwgroup.org_systems_core-v-mcu_0/model-lib-sim-verilator/obj_dir
```

### Building the Embdebug target for CORE-V

Created a build directory, we'll assume at the top level

```
mkdir bd
```

Configure with CMake, specifying the location of the Embedebug `include`
directory and the Verilator model directory.  You may need to change the
precise details depending on your directory hierarchy.

```
cmake -DCMAKE_INSTALL_PREFIX=<install-dir> \
    -DEMBDEBUG_INCLUDE_DIR=<embdebug-dir>/include \
    -DCV_MCU_BUILD_DIR=<model-dir>
```

- `<install-dir>` is the directory where you want the generated library
  stored (optional).
- `<embdebug-dir>` is the root directory of the generic Embddebug engine.
- `<model-dir>` is the location of the generated MCU verilator
  model. Typically something like
  `build/openhwgroup.org_systems_core-v-mcu_0/model-lib-verilator/obj_dir`
  within the `core-v-mcu` repository.

Build and install using CMake
```
cmake --build . -- -j $(nproc)
cmake --build . --target install
```

(Only install if you have specified `-DCMAKE_INSTALL_PREFIX`).

### A Caveat

The CORE-V MCU code initializes its boot ROM by using `$readmemh` with a relative file name.  This means you need the `mem_init` directory to be in the same directory from which you run Embdebug.  A workaround to make Embdebug more usable is to edit the CORE-V MCU code to use an absolute file name witnin `$readmemh`.

## Running Embdebug

Once built and installed, you can start the server using
```
embdebug --soname cv32e40
```
This will start the server, with the CV32E40P model and report a port name on which to connect.  Use `embdebug --help` to see other options which are accessible.  You can then connect from RISC-V GDB using:
```
(gdb) target remote :<pornum>
```
Where `<portnum>` is the port number reported when starting Embdebug.

It is also possible to start embdebug directly from within gdb and connect via a pipe.  In this case there is no need to run Embdebug itself.  From within GDB, you can just use
````
(gdb) target remote | embdebug --soname cv32e40
```
