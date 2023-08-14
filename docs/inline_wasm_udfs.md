# Getting Started with inline WASM (C++) UDFs

This is a sample guide for how to generate a UDF JS with inline WASM from C++.

## Overview

We will be using a provided bazel macro under
[`//tools/inline_wasm/wasm.bzl`](/tools/inline_wasm/wasm.bzl) to generate a UDF delta file.

The macro uses
[Embind](https://emscripten.org/docs/porting/connecting_cpp_and_javascript/embind.html) to generate
a WASM binary and its accompanying JavaScript.

Using Embind requires including C++ code that exposes C++ to JavaScript. Similarly, the JavaScript
needs to instantiate a WASM module and access it through a variable.

This guide will not cover usage of Emscripten/Embind beyond what is needed for the examples under
`//tools/inline_wasm/examples`. Please browse through the
[Emscripten documentation](https://emscripten.org/docs/introducing_emscripten/index.html) for
detailed usage.

## Example

Complete example is at `//tools/inline_wasm/examples/hello_world`.

### Write C++ code

For our example, we have a simple `HelloClass` with a static `SayHello` function.

```C++
#include "emscripten/bind.h"

class HelloClass {
 public:
  static std::string SayHello(const std::string& name) { return "Yo! " + name; }
};

EMSCRIPTEN_BINDINGS(Hello) {
  emscripten::class_<HelloClass>("HelloClass")
      .constructor<>()
      .class_function("SayHello", &HelloClass::SayHello);
}
```

Write a `cc_binary` target with at least the required options:

```bazel
BASE_LINKOPTS = [
    # REQUIRED
    # Enable embind
    "--bind",
    "-s MODULARIZE=1",
    "-s EXPORT_NAME=wasmModule",

    # OPTIONAL
    # no main function
    "--no-entry",
    # optimization
    "-O3",
    # Do not use closure. We probably want to use closure eventually.
    "--closure=0",
    # Disable the filesystem.
    "-s FILESYSTEM=0",
]

cc_binary(
    name = "hello_cc",
    srcs = ["hello.cc"],
    linkopts = BASE_LINKOPTS,
    # This target won't build successfully on its own because of missing emscripten
    # headers etc. Therefore, we hide it from wildcards.
    tags = ["manual"],
)
```

### Write JS code

The provided bazel macro will include the following function in the final JavaScript output.

```javascript
async function getModule() {
    var Module = {
        instantiateWasm: function (imports, successCallback) {
            var module = new WebAssembly.Module(wasm_array);
            var instance = new WebAssembly.Instance(module, imports);
            Module.testWasmInstantiationSucceeded = 1;
            successCallback(instance);
            return instance.exports;
        },
    };
    return await wasmModule(Module);
}
```

You will need to call this function from your custom UDF to access your C++ bindings.

Example:

```javascript
const module = await getModule();
module.HelloClass.SayHello('hi');
```

### Generate a UDF delta file

We have a bazel macro for generating a UDF file given a C++ BUILD target and a custom UDF
JavaScript. The output will be under the `dist/` directory.

-   Define your BUILD target:

```bazel
load("//tools/inline_wasm:wasm.bzl", "inline_wasm_udf_delta")

inline_wasm_udf_delta (
    name = "hello_delta",
    cc_target = ":hello_cc",
    custom_udf_js = "my_udf.js",
    custom_udf_js_handler = "HandleRequest",
    output_file_name = "DELTA_0000000000000001"
)
```

-   Generate the DELTA file:

```shell
bazel run --incompatible_enable_cc_toolchain_resolution path/to/udf_delta_target:hello_delta
```

-   Get the DELTA file from `dist/`.