<div align="center">

# Calculate Pi · 计算圆周率 π

[![CI](https://github.com/<你的用户名>/Calculate%20Pi/actions/workflows/ci.yml/badge.svg)](https://github.com/<你的用户名>/Calculate%20Pi/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![CMake](https://img.shields.io/badge/CMake-3.16+-064F8C.svg)](https://cmake.org/)

</div>

> **中文**：一个基于 **Chudnovsky 二进制分裂算法** 的高精度 π 计算工具，使用 **C++20** 与 **CMake** 实现，支持命令行与交互式两种使用模式。
>
> **English**: A high-precision π calculator based on the **Chudnovsky binary splitting algorithm**, implemented in **C++20** with **CMake**. It supports both command-line and interactive modes.

---

## 📸 运行截图 / Screenshots

> 以下输出均为程序真实运行结果捕获。/ The outputs below are captured from real program executions.

### 帮助信息 / Help

![Help screenshot](assets/screenshot_help.svg)

```text
Calculate Pi - Chudnovsky binary splitting

Usage:
  CalculatePi [options]
  CalculatePi --terms <N> [-o <path>] [--stats]

Options:
  -n, --terms <N>   Number of Chudnovsky series terms (required in CLI mode)
  -o, --output <path>  Write the result to the specified file
      --stats       Print additional statistics
  -h, --help        Show this help message

If no arguments are given, the program enters interactive mode.
```

### 计算 100 项 / Compute 100 terms

![Compute 100 terms screenshot](assets/screenshot_compute.svg)

```text
3.1415926535897932384626433832795028841971693993751058209749445923078164062862089986280348253421170679821480865132823066470938446095505822317253594081284811174502841027019385211055596446229489549303819644288109756659334461284756482337867831652712019091456485669234603486104543266482133936072602491412737245870066063155881748815209209628292540917153643678925903600113305305488204665213841469519415116094330572703657595919530921861173819326117931051185480744623799627495673518857527248912279381830119491298336733624406566430860213949463952247371907021798609437027705392171762931767523846748184676694051320005681271452635608277857713427577896091736371787214684409012249534301465495853710507922796892589235420199561121290219608640344181598136297747713099605187072113499999983729780499510597317328160963185950244594553469083026425223082533446850352619311881710100031378387528865875332083814206171776691473035982534904287554687311595628638823537875937519577818577805321712268066130019278766111959092164201989380952572010654858632788659361533818279682303019520353018529689957736225994138912497217752834791315155748572424541506959508295331168617278558890750983817546374649393192550604009277016711390098488240128583616035637076601047101819429555961989467678374494482553797747268471040475346462080466842590694912933136770289891521047521620569660240580381501935112533824300355876402474964732639141992726042699227967823547816360093
Terms: 100
Digits: 1417
Time: 0 ms
```

### 保存 1000 项结果到文件 / Save 1000 terms to file

```bash
./build/calculate_pi --terms 1000 --output pi_1000.txt --stats
```

```text
3.1415926535897932384626433832795028841971693993751058209749445923078164062862089986280348253421170679821480865132823066...
Terms: 1000
Digits: 14180
Time: 4 ms
```

---

## ✨ 功能特性 / Features

| 中文 | English |
|------|---------|
| 使用 Chudnovsky 二进制分裂算法计算 π | Computes π using the Chudnovsky binary splitting algorithm |
| 自研大整数 (`BigInt`)，支持 Naive、Karatsuba、FFT 与自适应乘法策略 | Custom `BigInt` with Naive, Karatsuba, FFT, and adaptive multiplication strategies |
| 基于 `std::thread` 的线程池实现并行递归分裂 | Thread pool based on `std::thread` for parallel recursive splitting |
| 支持命令行参数与交互式两种运行模式 | Supports both command-line arguments and interactive modes |
| 支持将计算结果保存到文件 | Supports saving results to a file |
| 跨平台：Windows、Linux、macOS | Cross-platform: Windows, Linux, macOS |

---

## 🛠️ 系统要求 / Requirements

- **中文**：C++20 兼容编译器、CMake 3.16 或更高版本、支持 `std::thread` 的平台。
- **English**: A C++20-compatible compiler, CMake 3.16 or later, and a platform supporting `std::thread`.

测试过的环境 / Tested environments:

- Windows 11 + MSVC / MinGW-w64
- Ubuntu 22.04 + GCC 11
- macOS 14 + Clang 15

---

## 🚀 构建与运行 / Build and Run

```bash
# 配置 / Configure
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release

# 构建 / Build
cmake --build build --config Release

# 运行 / Run
./build/calculate_pi --terms 100 --stats        # Linux / macOS
./build/calculate_pi.exe --terms 100 --stats    # Windows
```

---

## 📖 使用方式 / Usage

### 命令行模式 / Command-line mode

```bash
# 显示帮助 / Show help
./build/calculate_pi --help

# 计算 N 项并输出到控制台 / Compute N terms and print to console
./build/calculate_pi --terms 100

# 同时显示统计信息 / Show statistics
./build/calculate_pi --terms 100 --stats

# 将结果保存到文件 / Save result to file
./build/calculate_pi --terms 1000 --output pi.txt --stats
```

### 交互模式 / Interactive mode

不传递任何参数即可进入交互模式：

```bash
./build/calculate_pi
```

程序会依次询问：

1. 项数 `N`（正整数）
2. 是否保存到文件
3. 是否显示统计信息

Run without arguments to enter interactive mode. The program will ask for:

1. Number of terms `N` (positive integer)
2. Whether to save to a file
3. Whether to show statistics

---

## 🏗️ 项目结构 / Project Structure

```text
Calculate Pi
├── CMakeLists.txt          # CMake 构建配置 / Build configuration
├── LICENSE                 # MIT 许可证 / MIT license
├── README.md               # 本文件 / This file
├── main.cpp                # 程序入口 / Program entry point
├── include/cpi/            # 头文件 / Header files
│   ├── app.hpp             # 应用主逻辑 / Application logic
│   ├── bigint.hpp          # 大整数 / Big integer
│   ├── bigint_multiply.hpp # 乘法策略 / Multiplication strategies
│   ├── chudnovsky.hpp      # Chudnovsky 算法 / Chudnovsky algorithm
│   ├── cli.hpp             # 命令行解析 / CLI parsing
│   ├── concurrency.hpp     # 线程池 / Thread pool
│   ├── expected.hpp        # 轻量期望类型 / Lightweight expected type
│   └── io.hpp              # 输入输出 / I/O utilities
├── src/                    # 源文件 / Source files
│   ├── app.cpp
│   ├── bigint.cpp
│   ├── bigint_multiply.cpp
│   ├── chudnovsky.cpp
│   ├── cli.cpp
│   ├── concurrency.cpp
│   └── io.cpp
└── .github/workflows/
    └── ci.yml              # GitHub Actions 持续集成 / CI workflow
```

---

## 🧮 算法说明 / Algorithm Notes

本项目使用 [Chudnovsky 算法](https://en.wikipedia.org/wiki/Chudnovsky_algorithm) 计算 π：

```
1/π = 12 * Σ ((-1)^k * (6k)! * (13591409 + 545140134k)) / ((3k)! * (k!)^3 * 640320^(3k + 3/2))
```

This project uses the [Chudnovsky algorithm](https://en.wikipedia.org/wiki/Chudnovsky_algorithm) to compute π:

```
1/π = 12 * Σ ((-1)^k * (6k)! * (13591409 + 545140134k)) / ((3k)! * (k!)^3 * 640320^(3k + 3/2))
```

实现中采用 **二进制分裂**（binary splitting）将级数求和转化为递归的分治计算，并通过线程池对递归任务进行并行化，从而高效获得上百万位精度。

The implementation uses **binary splitting** to transform the series summation into recursive divide-and-conquer computations, parallelizing recursive tasks via a thread pool to efficiently obtain millions of digits of precision.

---

## 🤝 贡献 / Contributing

欢迎提交 Issue 和 Pull Request！

Issues and Pull Requests are welcome!

---

## 📄 许可证 / License

本项目采用 [MIT 许可证](LICENSE) 开源。

This project is open-sourced under the [MIT License](LICENSE).

Copyright (c) 2026 Calculate Pi project
