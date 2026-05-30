// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/system/sys_info.h"

#include <stddef.h>
#include <stdint.h>
#include <sys/param.h>
#include <sys/shm.h>
#include <sys/sysctl.h>

#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/posix/sysctl.h"
#include "base/strings/string_util.h"

namespace base {

namespace {

ByteSize AmountOfMemory(int pages_name) {
  long pages = sysconf(pages_name);
  long page_size = sysconf(_SC_PAGESIZE);
  if (pages < 0 || page_size < 0) {
    return ByteSize(0);
  }
  return ByteSize(checked_cast<unsigned long>(page_size)) * pages;
}

}  // namespace

// pledge(2)
ByteSize aofpmem = ByteSize(0);
uint64_t shmmax = 0;
char cpumodel[256];

// static
int SysInfo::NumberOfProcessors() {
  int mib[] = {CTL_HW, HW_NCPUONLINE};
  int ncpu;
  size_t size = sizeof(ncpu);
  if (sysctl(mib, std::size(mib), &ncpu, &size, NULL, 0) < 0) {
    NOTREACHED();
  }
  return ncpu;
}

// static
std::string SysInfo::CPUModelName() {
  int mib[] = {CTL_HW, HW_MODEL};
  size_t len = std::size(cpumodel);
  
  if (cpumodel[0] == '\0') {
    if (sysctl(mib, std::size(mib), cpumodel, &len, NULL, 0) < 0)
      return std::string();
  }
 
  return std::string(cpumodel, len - 1);
}

// static
ByteSize SysInfo::AmountOfTotalPhysicalMemoryImpl() {
  // pledge(2)
  if (aofpmem == ByteSize(0))
    aofpmem = AmountOfMemory(_SC_PHYS_PAGES);
  return aofpmem;
}

// static
ByteSize SysInfo::AmountOfAvailablePhysicalMemoryImpl() {
  // We should add inactive file-backed memory also but there is no such
  // information from OpenBSD unfortunately.
  return AmountOfMemory(_SC_AVPHYS_PAGES);
}

// static
uint64_t SysInfo::MaxSharedMemorySize() {
  int mib[] = {CTL_KERN, KERN_SHMINFO, KERN_SHMINFO_SHMMAX};
  size_t limit;
  size_t size = sizeof(limit);
  // pledge(2)
  if (shmmax)
    goto out;
  if (sysctl(mib, std::size(mib), &limit, &size, NULL, 0) < 0) {
    NOTREACHED();
  }
  shmmax = static_cast<uint64_t>(limit);
out:
  return shmmax;
}

// static
SysInfo::HardwareInfo SysInfo::GetHardwareInfoSync() {
  HardwareInfo info;
  // Set the manufacturer to "OpenBSD" and the model to
  // an empty string.
  info.manufacturer = "OpenBSD";
  info.model = HardwareModelName();
  DCHECK(IsStringUTF8(info.manufacturer));
  DCHECK(IsStringUTF8(info.model));
  return info;
}

}  // namespace base
