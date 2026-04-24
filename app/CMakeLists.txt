#
# Copyright (c) 2021 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#
cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(NONE)

# NORDIC SDK APP START
target_sources(app PRIVATE
  src/main.c
  src/input.c
)

target_include_directories(app PRIVATE src)

target_compile_options(app PRIVATE -Wall -Wextra -Werror)

# NORDIC SDK APP END
