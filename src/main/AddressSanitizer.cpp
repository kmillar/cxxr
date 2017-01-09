/*
 *  R : A Computer Language for Statistical Data Analysis
 *  Copyright (C) 2016 and onwards the Rho Project Authors.
 *
 *  Rho is not part of the R project, and bugs and other issues should
 *  not be reported via r-bugs or other R project channels; instead refer
 *  to the Rho website.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2.1 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, a copy is available at
 *  http://www.r-project.org/Licenses/
 */

#include "rho/AddressSanitizer.hpp"
#include <stdio.h>

#if HAVE_ASAN && defined(__clang__) && defined(STORE_ASAN_TRACES)

// Re-declare some sanitizer internals.
namespace __sanitizer {

typedef unsigned long uptr;
typedef uint32_t u32;

struct StackTrace {
  const uptr *trace;
  u32 size;
  u32 tag;

  void Print() const;
  static uptr GetCurrentPc();
};

struct BufferedStackTrace : public StackTrace {
  // Lots of storage.
  // All we care about here is that this is at least as large as the real
  // implementation.
  uptr storage[512];

  BufferedStackTrace() {
    trace = storage;
    size = 0;
    tag = 0;
  }

#if __clang_major__ > 3 || (__clang_major__ == 3 && __clang_minor__ >= 7)
  // Used in 3.7.0 and onwards.
  void SlowUnwindStack(uptr pc, u32 max_depth);
#else
  void SlowUnwindStack(uptr pc, uptr max_depth);
#endif
};

u32 StackDepotPut(StackTrace stack);
StackTrace StackDepotGet(u32 id);

}  // End of declarations of sanitizer internals.
using namespace __sanitizer;

u32 __asan_store_stacktrace() {
  BufferedStackTrace stack;
  unsigned max_depth = 20;  // Must be at least 3.
  stack.SlowUnwindStack(StackTrace::GetCurrentPc(), max_depth);

  return StackDepotPut(stack);
}

void __asan_print_stacktrace(u32 trace_id) {
  StackDepotGet(trace_id).Print();
}

#else
namespace __sanitizer {
    typedef unsigned long uptr;
    typedef uint32_t u32;
}
using namespace __sanitizer;

u32 __asan_store_stacktrace() {
    return 0;
}

void __asan_print_stacktrace(u32) {
    printf("need to define STORE_ASAN_TRACES to get this stack trace\n");
}
#endif
