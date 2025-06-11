# GF2-Decrypter

## 编译要求

+ CMake 3.21及以上
+ 支持C++20的编译器
+ Git

## 第三方库

+ [CLI11](https://github.com/CLIUtils/CLI11)
+ [thread-pool](https://github.com/bshoshany/thread-pool)

如果启用了文本的解码，则还需要：

+ [nanopb](https://github.com/nanopb/nanopb)

并且编译需要额外的环境：

+ `Python3`
    + `protobuf`
    + `grpcio-tools`

