/*
 * This file is part of Licq, an instant messaging client for UNIX.
 * Copyright (C) 2007-2010 Erik Johansson <erijo@licq.org>
 *
 * Licq is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Licq is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Licq; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "dynamiclibrary.h"

#include <boost/exception/errinfo_api_function.hpp>
#include <boost/exception/info.hpp>
#include <boost/exception/get_error_info.hpp>
#include <cassert>
#include <dlfcn.h>
#include <string>

using namespace LicqDaemon;

typedef
boost::error_info<struct tag_errinfo_sys_error, std::string> errinfo_sys_error;

DynamicLibrary::Exception::
Exception(const char* apiFunction, const char* error)
{
  *this << boost::errinfo_api_function(apiFunction)
        << errinfo_sys_error(error);
}

std::string DynamicLibrary::Exception::getSystemError() const
{
  return *boost::get_error_info<errinfo_sys_error>(*this);
}

DynamicLibrary::DynamicLibrary(const std::string& filename)
  : myName(filename),
    myDlHandle(NULL)
{
  // Clear any old error conditions
  ::dlerror();

  if (filename.empty())
    myDlHandle = ::dlopen(NULL, DLOPEN_POLICY);
  else
    myDlHandle = ::dlopen(filename.c_str(), DLOPEN_POLICY);

  if (myDlHandle == NULL)
    LICQ_THROW(Exception("dlopen", ::dlerror()));
}

DynamicLibrary::~DynamicLibrary() throw()
{
  ::dlclose(myDlHandle);
}

template<> void
DynamicLibrary::getSymbol(const std::string& name, void** symbol)
{
  assert(symbol != NULL);

  // Clear any old error conditions
  ::dlerror();

  *symbol = ::dlsym(myDlHandle, name.c_str());

  const char* error = ::dlerror();
  if (error != NULL)
    LICQ_THROW(Exception("dlsym", error));
}
