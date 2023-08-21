// RUN: %libomp-compile-and-run | %sort-threads | FileCheck %S/base_serialized.h
// REQUIRES: ompt
// GCC doesn't call runtime for auto = static schedule
// XFAIL: gcc

#define SCHEDULE auto
#include "base_serialized.h"
