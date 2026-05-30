// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines base::PathProviderPosix, default path provider on POSIX OSes that
// don't have their own base_paths_OS.cc implementation (i.e. all but Mac and
// Android).

#include "base/base_paths.h"

#include <limits.h>
#include <stddef.h>

#include <memory>
#include <ostream>
#include <string>

#include "base/command_line.h"
#include "base/environment.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/nix/xdg_util.h"
#include "base/notreached.h"
#include "base/path_service.h"
#include "base/posix/sysctl.h"
#include "base/process/process_metrics.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_BSD)
#include <sys/param.h>
#include <sys/sysctl.h>
#if BUILDFLAG(IS_OPENBSD)
#include <kvm.h>
#define MAXTOKENS 2
#endif
#elif BUILDFLAG(IS_SOLARIS) || BUILDFLAG(IS_AIX)
#include <stdlib.h>
#endif

namespace base {

bool PathProviderPosix(int key, FilePath* result) {
  switch (key) {
    case FILE_EXE:
    case FILE_MODULE: {  // TODO(evanm): is this correct?
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
      FilePath bin_dir;
      if (!ReadSymbolicLink(FilePath(kProcSelfExe), &bin_dir)) {
        NOTREACHED() << "Unable to resolve " << kProcSelfExe << ".";
      }
      *result = bin_dir;
      return true;
#elif BUILDFLAG(IS_FREEBSD)
      std::optional<std::string> bin_dir = StringSysctl({ CTL_KERN, KERN_PROC, KERN_PROC_PATHNAME, -1 });
      if (!bin_dir.has_value() || bin_dir.value().length() <= 1) {
        NOTREACHED() << "Unable to resolve path.";
      }
      *result = FilePath(bin_dir.value());
      return true;
#elif BUILDFLAG(IS_SOLARIS)
      char bin_dir[PATH_MAX + 1];
      if (realpath(getexecname(), bin_dir) == NULL) {
        NOTREACHED() << "Unable to resolve " << getexecname() << ".";
      }
      *result = FilePath(bin_dir);
      return true;
#elif BUILDFLAG(IS_OPENBSD) || BUILDFLAG(IS_AIX)
      char *cpath;
#if !BUILDFLAG(IS_AIX)
      struct kinfo_file *files;
      kvm_t *kd = NULL;
      char errbuf[_POSIX2_LINE_MAX];
      static char retval[PATH_MAX];
      int cnt;
      struct stat sb;
      pid_t cpid = getpid();
      bool ret = false;

      const base::CommandLine* command_line =
          base::CommandLine::ForCurrentProcess();

      VLOG(1) << "PathProviderPosix argv: " << command_line->argv()[0];

      if (realpath(command_line->argv()[0].c_str(), retval) == NULL)
        goto out;

      if (stat(command_line->argv()[0].c_str(), &sb) < 0)
        goto out;

      if (!command_line->HasSwitch("no-sandbox")) {
        ret = true;
        *result = FilePath(retval);
        VLOG(1) << "PathProviderPosix (sandbox) result: " << retval;
        goto out;
      }

      if ((kd = kvm_openfiles(NULL, NULL, NULL, (int)KVM_NO_FILES, errbuf)) == NULL)
        goto out;

      if ((files = kvm_getfiles(kd, KERN_FILE_BYPID, cpid,
                                sizeof(struct kinfo_file), &cnt)) == NULL)
        goto out;

      for (int i = 0; i < cnt; i++) {
        if (files[i].fd_fd == KERN_FILE_TEXT &&
            files[i].va_fsid == static_cast<uint32_t>(sb.st_dev) &&
            files[i].va_fileid == sb.st_ino) {
          ret = true;
          *result = FilePath(retval);
          VLOG(1) << "PathProviderPosix result: " << retval;
        }
      }
out:
      if (kd)
        kvm_close(kd);
      if (!ret) {
#endif
        if ((cpath = getenv("CHROME_EXE_PATH")) != NULL)
          *result = FilePath(cpath);
        else
          *result = FilePath("/usr/local/chrome/chrome");
        return true;
#if !BUILDFLAG(IS_AIX)
      }
      return ret;
#endif
#endif
    }
    case DIR_SRC_TEST_DATA_ROOT: {
      FilePath path;
      // On POSIX, unit tests execute two levels deep from the source root.
      // For example:  out/{Debug|Release}/net_unittest
      if (PathService::Get(DIR_EXE, &path)) {
        *result = path.DirName().DirName();
        return true;
      }

      DLOG(ERROR) << "Couldn't find your source root.  "
                  << "Try running from your chromium/src directory.";
      return false;
    }
    case DIR_USER_DESKTOP:
      *result = nix::GetXDGUserDirectory("DESKTOP", "Desktop");
      return true;
    case DIR_CACHE: {
      std::unique_ptr<Environment> env(Environment::Create());
      FilePath cache_dir(
          nix::GetXDGDirectory(env.get(), "XDG_CACHE_HOME", ".cache"));
      *result = cache_dir;
      return true;
    }
  }
  return false;
}

}  // namespace base
