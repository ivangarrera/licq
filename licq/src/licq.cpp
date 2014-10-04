/*
 * This file is part of Licq, an instant messaging client for UNIX.
 * Copyright (C) 1998-2014 Licq developers <licq-dev@googlegroups.com>
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

#include <boost/foreach.hpp>
#include <cassert>
#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <fcntl.h>
#include <getopt.h>
#include <iostream>
#include <pwd.h>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "licq.h"
#include <licq/logging/log.h>
#include <licq/logging/logutils.h>
#include <licq/exceptions/exception.h>
#include <licq/inifile.h>
#include <licq/version.h>

#include "contactlist/usermanager.h"
#include "daemon.h"
#include "filter.h"
#include "gettext.h"
#include "gpghelper.h"
#include "logging/logservice.h"
#include "logging/streamlogsink.h"
#include "oneventmanager.h"
#include "plugin/pluginmanager.h"
#include "sarmanager.h"
#include "statistics.h"

#ifdef USE_FIFO
#include "fifo.h"
#endif

using Licq::GeneralPlugin;
using Licq::ProtocolPlugin;
using Licq::gLog;
using LicqDaemon::Daemon;
using LicqDaemon::PluginManager;
using LicqDaemon::gDaemon;
#ifdef USE_FIFO
using LicqDaemon::gFifo;
#endif
using LicqDaemon::gFilterManager;
using LicqDaemon::gGpgHelper;
using LicqDaemon::gLogService;
using LicqDaemon::gOnEventManager;
using LicqDaemon::gSarManager;
using LicqDaemon::gPluginManager;
using LicqDaemon::gStatistics;
using LicqDaemon::gUserManager;
using std::list;
using std::string;

/*-----Start OpenSSL code--------------------------------------------------*/

#ifdef USE_OPENSSL
#include <openssl/ssl.h>
#include <openssl/dh.h>
#include <openssl/opensslv.h>

extern SSL_CTX *gSSL_CTX;
extern SSL_CTX *gSSL_CTX_NONICQ;

// AUTOGENERATED by dhparam
static DH *get_dh512()
        {
        static unsigned char dh512_p[]={
                0xFF,0xD3,0xF9,0x7C,0xEB,0xFE,0x45,0x2E,0x47,0x41,0xC1,0x8B,
                0xF7,0xB9,0xC6,0xF2,0x40,0xCF,0x10,0x8B,0xF3,0xD7,0x08,0xC7,
                0xF0,0x3F,0x46,0x7A,0xAD,0x71,0x6A,0x70,0xE1,0x76,0x8F,0xD9,
                0xD4,0x46,0x70,0xFB,0x31,0x9B,0xD8,0x86,0x58,0x03,0xE6,0x6F,
                0x08,0x9B,0x16,0xA0,0x78,0x70,0x6C,0xB1,0x78,0x73,0x52,0x3F,
                0xD2,0x74,0xED,0x9B,
                };
        static unsigned char dh512_g[]={
                0x02,
                };
        DH *dh;

        if ((dh=DH_new()) == NULL) return(NULL);
        dh->p=BN_bin2bn(dh512_p,sizeof(dh512_p),NULL);
        dh->g=BN_bin2bn(dh512_g,sizeof(dh512_g),NULL);
        if ((dh->p == NULL) || (dh->g == NULL))
                { DH_free(dh); return(NULL); }
        return(dh);
        }

#ifdef SSL_DEBUG
void ssl_info_callback(SSL *s, int where, int ret)
{
    const char *str;
    int w;

    w = where & ~SSL_ST_MASK;

    if (w & SSL_ST_CONNECT) str="SSL_connect";
    else if (w & SSL_ST_ACCEPT) str="SSL_accept";
    else str="undefined";

    if (where & SSL_CB_LOOP)
    {
        gLog.info("%s%s:%s\n",L_SSLxSTR,str,SSL_state_string_long(s));
    }
    else if (where & SSL_CB_ALERT)
    {
        str=(where & SSL_CB_READ)?"read":"write";
        gLog.info("%sSSL3 alert %s:%s:%s\n",L_SSLxSTR,
            str,
            SSL_alert_type_string_long(ret),
            SSL_alert_desc_string_long(ret));
    }
    else if (where & SSL_CB_EXIT)
    {
        if (ret == 0)
            gLog.info("%s%s:failed in %s\n",L_SSLxSTR,
                str,SSL_state_string_long(s));
        else if (ret < 0)
        {
            gLog.info("%s%s:error in %s\n",L_SSLxSTR,
                str,SSL_state_string_long(s));
        }
    }
}
#endif
#endif
/*-----End of OpenSSL code-------------------------------------------------*/

// Sets up BASE_DIR to be the absolute path to the user's config base
// directory. BASE_DIR will both start and end with a slash (/).
static bool setupBaseDirPath(const std::string& path)
{
  if (path.size() > 0 && path[0] == '/')
    gDaemon.setBaseDir(path);
  else
  {
    // Get current working directory
    // Linux uses PATH_MAX, BSD uses MAXPATHLEN, either seems to work on both
#ifdef PATH_MAX
    char cwd[PATH_MAX];
#else
    // PATH_MAX is not defined on GNU Hurd
    char cwd[4096];
#endif
    if (::getcwd(cwd, sizeof(cwd)) == NULL)
    {
      ::fprintf(stderr, tr("Could not get current working directory\n"));
      return false;
    }

    // Tidy up path if it is a "simple" relative path
    string newPath;
    if (path.size() >= 2 && path[0] == '.' && path[1] == '/')
      newPath = path.substr(1);
    else
      newPath = '/' + path;

    // Construct an absolute path
    gDaemon.setBaseDir(cwd + newPath);
  }

  return (gDaemon.baseDir().size() > 0);
}

/**
 * Prints the @a error to stderr (by means of gLog), and if the user is running
 * X, tries to show a dialog with the error.
 */
void displayFatalError(const string& error, int useLicqLog)
{
  if (useLicqLog)
    gLog.error("%s", error.c_str());
  else
    fprintf(stderr, "\n%s\n", error.c_str());

  // Try to show the error if we're running X
  if (getenv("DISPLAY") != NULL)
  {
    pid_t child = fork();
    if (child == 0)
    {
      // execlp never returns (except on error).
      execlp("kdialog", "kdialog", "--error", error.c_str(), NULL);
      execlp("Xdialog", "Xdialog", "--title", "Error", "--msgbox", error.c_str(), "0", "0", NULL);
      execlp("xmessage", "xmessage", "-center", error.c_str(), NULL);

      exit(EXIT_FAILURE);
    }
    else if (child != -1)
    {
      int status;
      waitpid(child, &status, 0);
    }
  }
}

void handleExitSignal(int signal)
{
  gLog.info(tr("Received signal %d, exiting"), signal);
  gDaemon.Shutdown();
}

char **global_argv = NULL;
int global_argc = 0;

CLicq::CLicq() :
  myConsoleLogLevel(0)
{
}

bool CLicq::Init(int argc, char **argv)
{
  myConsoleLog.reset(new LicqDaemon::StreamLogSink(std::cerr));
  myConsoleLog->setLogLevel(Licq::Log::Error, true);
  myConsoleLog->setLogLevel(Licq::Log::Warning, true);
  gLogService.registerDefaultLogSink(myConsoleLog);

  gDaemon.preInitialize(this);

  string redirect;
  list<string> generalPlugins;
  list<string> protocolPlugins;

  // parse command line for arguments
  bool bHelp = false;
  bool bFork = false;
  bool bBaseDir = false;
  bool bForceInit = false;
  bool bRedirect_ok = false;
  bool bUseColor = true;
  // Check the no one is trying session management on us
  if (argc > 1 && strcmp(argv[1], "-session") == 0)
  {
    fprintf(stderr, tr("Session management is not supported by Licq.\n"));
  }
  else
  {
    int i = 0;
#ifdef __GLIBC__
    while( (i = getopt(argc, argv, "--hd:b:p:l:Io:fcv")) > 0)
#else
    while( (i = getopt(argc, argv, "hd:b:p:l:Io:fcv")) > 0)
#endif
    {
      switch (i)
      {
      case 'h':  // help
        PrintUsage();
        bHelp = true;
        break;
      case 'b':  // base directory
        if (!setupBaseDirPath(optarg))
          return false;
        bBaseDir = true;
        break;
      case 'd':  // debug level
        myConsoleLogLevel = atol(optarg);
        break;
      case 'c':  // use color
        bUseColor = false;
        break;
      case 'I':  // force init
        bForceInit = true;
        break;
        case 'p':  // new plugin
          generalPlugins.push_back(optarg);
          break;
        case 'l':  // new protocol plugin
          protocolPlugins.push_back(optarg);
          break;
        case 'o':  // redirect stderr
          redirect = optarg;
          break;
      case 'f':  // fork
        bFork = true;
        break;
      case 'v':  // show version
        printf(tr("%s version %s, compiled on %s\n"),
               PACKAGE, LICQ_VERSION_STRING, __DATE__);
        return false;
        break;
      }
    }
  }

  // Save the command line arguments in case anybody cares
  global_argc = argc;
  global_argv = argv;

  // Fork into the background
  if (bFork && fork()) exit(0);

  // See if redirection works, set bUseColor to false if we redirect
  // to a file.
  if (!redirect.empty())
  {
    int fd = open(redirect.c_str(), O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
    bRedirect_ok = (fd != -1 && dup2(fd, STDERR_FILENO) != -1);
  }

  if(!isatty(STDERR_FILENO))
    bUseColor = false;

  // Dump all initial errors and warnings to the console log regardless of the
  // requested debug level.
  myConsoleLog->setLogLevelsFromBitmask(
      Licq::LogUtils::convertOldBitmaskToNew(myConsoleLogLevel));
  myConsoleLog->setLogLevel(Licq::Log::Warning, true);
  myConsoleLog->setLogLevel(Licq::Log::Error, true);
  myConsoleLog->setUseColors(bUseColor);

  // Redirect stdout and stderr if asked to
  if (!redirect.empty()) {
    if (bRedirect_ok)
      gLog.info(tr("Output redirected to \"%s\""), redirect.c_str());
    else
      gLog.warning(tr("Redirection to \"%s\" failed: %s"), redirect.c_str(), strerror(errno));
  }

  // if no base directory set on the command line then get it from HOME
  if (!bBaseDir)
  {
    const char* home = ::getenv("HOME");
    if (home == NULL || home[0] != '/')
    {
      gLog.error(tr("$HOME not set or invalid; "
                    "unable to determine config base directory"));
      return false;
    }
    gDaemon.setBaseDir(string(home) + "/.licq");
  }

  // check if user has conf files installed, install them if not
  if ( (access(gDaemon.baseDir().c_str(), F_OK) < 0 || bForceInit) && !Install() )
    return false;

  // Load up the plugins
  BOOST_FOREACH(string& pluginName, generalPlugins)
  {
    GeneralPlugin::Ptr plugin = LoadPlugin(pluginName, argc, argv, !bHelp);
    if (!plugin)
      return false;
    if (bHelp)
    {
      fprintf(stderr, "----------\nLicq Plugin: %s %s\n%s\n",
          plugin->name().c_str(), plugin->version().c_str(), plugin->usage().c_str());
    }
  }
  BOOST_FOREACH(string& pluginName, protocolPlugins)
  {
    ProtocolPlugin::Ptr plugin = LoadProtoPlugin(pluginName, !bHelp);
    if (!plugin)
      return false;
    if (bHelp)
    {
      fprintf(stderr, "----------\nLicq Protocol Plugin: %s %s\n",
          plugin->name().c_str(), plugin->version().c_str());
    }
  }
  if (bHelp) return false;

  // Check pid
  // We do this by acquiring a write lock on the pid file and never closing the file.
  // When Licq is killed (normally or abnormally) the file will be closed by the operating
  // system and the lock released.
  char szKey[32];
  string pidfile = gDaemon.baseDir() + "licq.pid";

  // Never close pidFile!
  int pidFile = open(pidfile.c_str(), O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
  if (pidFile < 0)
  {
    // We couldn't open (or create) the file for writing.
    // If the file doesn't exists we continue without lockfile protection.
    // If it does exist, we bail out.
    struct stat buf;
    if (stat(pidfile.c_str(), &buf) < 0 && errno == ENOENT)
    {
      gLog.warning(tr("Licq: %s cannot be opened for writing,\n"
                      "      skipping lockfile protection."),
                   pidfile.c_str());
    }
    else
    {
      const size_t ERR_SIZE = 511;
      char error[ERR_SIZE + 1];

      // Try to read the pid of running Licq instance.
      FILE* fs = fopen(pidfile.c_str(), "r");
      if (fs != NULL)
      {
          fgets(szKey, 32, fs);
          pid_t pid = atol(szKey);

          snprintf(error, ERR_SIZE,
                   tr("Licq: Already running at pid %d.\n"
                      "      Kill process or remove %s."),
                   (int)pid, pidfile.c_str());
          fclose(fs);
      }
      else
      {
        snprintf(error, ERR_SIZE,
            tr("Licq: Unable to determine pid of running Licq instance."));
      }

      error[ERR_SIZE] = '\0';
      displayFatalError(error, 1);

      return false;
    }
  }
  else
  {
    struct flock lock;
    lock.l_type = F_WRLCK; // Write lock is exclusive lock
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0; // Lock file through to the end of file

    if (fcntl(pidFile, F_SETLK, &lock) != 0)
    {
      // Failed to get the lock => Licq is already running
      const size_t ERR_SIZE = 511;
      char error[ERR_SIZE + 1];

      if (fcntl(pidFile, F_GETLK, &lock) != 0)
      {
        snprintf(error, ERR_SIZE,
            tr("Licq: Unable to determine pid of running Licq instance."));
      }
      else
      {
        snprintf(error, ERR_SIZE,
            tr("Licq: Already running at pid %d."), (int)lock.l_pid);
      }

      error[ERR_SIZE] = '\0';
      displayFatalError(error, 1);

      return false;
    }
    else
    {
      // Save our pid in the file
      ftruncate(pidFile, 0);
      int size = snprintf(szKey, 32, "%d\n", (int)getpid());
      write(pidFile, szKey, (size > 32 ? 32 : size));
    }
  }

  // Open the config file
  Licq::IniFile& licqConf(gDaemon.getLicqConf());
  if (!licqConf.loadFile())
  {
    gLog.error(tr("Could not load config file '%s'"), licqConf.filename().c_str());
    gDaemon.releaseLicqConf();
    return false;
  }

  // Verify the version
  licqConf.setSection("licq");
  unsigned nVersion;
  licqConf.get("Version", nVersion, 0);
  if (nVersion < LICQ_VERSION)
  {
    try
    {
      // Perform upgrades as needed
      if (nVersion < LICQ_MAKE_VERSION(1, 2, 8))
        upgradeLicq128(licqConf);
      if (nVersion < LICQ_MAKE_VERSION(1, 8, 0))
        upgradeLicq18(licqConf);

      // Write new version
      licqConf.setSection("licq");
      licqConf.set("Version", LICQ_VERSION);
      licqConf.writeFile();
    }
    catch (...)
    {
      gLog.error(tr("Upgrade failed. Please save your licq directory and "
          "report this as a bug."));
      gDaemon.releaseLicqConf();
      return false;
    }
  }

  // Find and load the protocol plugins before the UI plugins
  if (protocolPlugins.empty())
  {
    unsigned nNumProtoPlugins = 0;
    if (licqConf.setSection("plugins", false)
        && licqConf.get("NumProtoPlugins", nNumProtoPlugins))
    {
      char szKey[20];
      for (unsigned i = 0; i < nNumProtoPlugins; i++)
      {
        string pluginName;
        sprintf(szKey, "ProtoPlugin%d", i+1);
        if (!licqConf.get(szKey, pluginName))
          continue;
        if (!LoadProtoPlugin(pluginName.c_str()))
        {
          gDaemon.releaseLicqConf();
          return false;
        }
      }
    }
  }


  // Find and load the plugins from the conf file
  if (generalPlugins.empty())
  {
    unsigned nNumPlugins = 0;
    string pluginName;
    if (licqConf.setSection("plugins", false) && licqConf.get("NumPlugins", nNumPlugins) && nNumPlugins > 0)
    {
      for (unsigned i = 0; i < nNumPlugins; i++)
      {
        sprintf(szKey, "Plugin%d", i + 1);
        if (!licqConf.get(szKey, pluginName))
          continue;

        bool loaded = LoadPlugin(pluginName, argc, argv);

        // Make upgrade from 1.4.x-1.8.x and older easier by automatically switching
        //   from kde4/qt4-gui to kde/qt-gui
        if (!loaded && (pluginName == "qt4-gui" || pluginName == "kde4-gui"))
        {
          if (pluginName == "kde4-gui")
          {
            gLog.warning(tr("Plugin kde4-gui is no longer available, trying to load kde-gui instead."));
            loaded = LoadPlugin("kde-gui", argc, argv);
          }
          if (!loaded)
          {
            gLog.warning(tr("Plugin %s is no longer available, trying to load qt-gui instead."), pluginName.c_str());
            loaded = LoadPlugin("qt-gui", argc, argv);
          }
        }

        if (!loaded)
        {
          gDaemon.releaseLicqConf();
          return false;
        }
      }
    }
    else  // If no plugins, try some defaults one by one
    {
      const char* plugins[] = {"qt-gui", "kde-gui"};
      unsigned short i = 0, size = sizeof(plugins) / sizeof(char*);

      GeneralPlugin::Ptr plugin;
      while (i < size && !plugin)
        plugin = LoadPlugin(plugins[i++], argc, argv);
      if (!plugin)
      {
        gDaemon.releaseLicqConf();
        return false;
      }
    }
  }

  gDaemon.releaseLicqConf();

#ifdef USE_OPENSSL
  // Initialize SSL
  SSL_library_init();
  gSSL_CTX = SSL_CTX_new(TLSv1_method());
  gSSL_CTX_NONICQ = SSL_CTX_new(TLSv1_method());
#if OPENSSL_VERSION_NUMBER >= 0x00905000L
  SSL_CTX_set_cipher_list(gSSL_CTX, "ADH:@STRENGTH");
#else
  SSL_CTX_set_cipher_list(gSSL_CTX, "ADH");
#endif

#ifdef SSL_DEBUG
  SSL_CTX_set_info_callback(gSSL_CTX, (void (*)(const SSL *,int, int))ssl_info_callback);
#endif
  SSL_load_error_strings();

  DH *dh = get_dh512();
  SSL_CTX_set_tmp_dh(gSSL_CTX, dh);
  DH_free(dh);
#endif

  // Start things going
  if (!LicqDaemon::gUserManager.Load())
    return false;
  gDaemon.initialize();
  gGpgHelper.initialize();
  gOnEventManager.initialize();
  gSarManager.initialize();
  gStatistics.initialize();
  gFilterManager.initialize();

  return true;
}

CLicq::~CLicq()
{
#ifdef USE_FIFO
  gFifo.shutdown();
#endif

  gLogService.unregisterLogSink(myConsoleLog);
}


const char *CLicq::Version()
{
  static const char version[] = LICQ_VERSION_STRING;
  return version;
}


/*-----------------------------------------------------------------------------
 * LoadPlugin
 *
 * Loads the given plugin using the given command line arguments.
 *---------------------------------------------------------------------------*/
GeneralPlugin::Ptr CLicq::LoadPlugin(const string& name, int argc, char** argv, bool keep)
{
  // Set up the argument vector
  static int argcndx = 0;
  int argccnt = 0;
  // Step up to the first delimiter if we have done nothing yet
  if (argcndx == 0)
  {
    while (++argcndx < argc && strcmp(argv[argcndx], "--") != 0)
      ;
  }
  if (argcndx < argc)
  {
    while (++argcndx < argc && strcmp(argv[argcndx], "--") != 0)
      argccnt++;
  }
  return gPluginManager.loadGeneralPlugin(name, argccnt,
      &argv[argcndx - argccnt], keep);
}


ProtocolPlugin::Ptr CLicq::LoadProtoPlugin(const string& name, bool keep)
{
  return gPluginManager.loadProtocolPlugin(name, keep);
}

void CLicq::rawFileEvent(int /*id*/, int /*fd*/, int /*revents*/)
{
  switch (myPipe.getChar())
  {
    case NotifyReapPlugin:
      gPluginManager.reapPlugin();

      // Exit when there are no plugins left running
      if (gPluginManager.pluginCount() == 0)
        myMainLoop.quit();
      break;

    case NotifyShuttingDown:
      // Time to quit, but wait for plugins to shut down first
      myMainLoop.addTimeout(PluginManager::MAX_WAIT_PLUGIN * 1000, this, 2);
      // Stop flushing statistics
      myMainLoop.removeTimeout(1);
      break;
  }
}

void CLicq::timeoutEvent(int id)
{
  switch (id)
  {
    case 1:
      // Flush statistics data regulary
      gStatistics.flush();
      break;

    case 2:
      // Timeout waiting for plugins to shut down
      myMainLoop.quit();
      break;
  }
}

int CLicq::Main()
{
  if (gPluginManager.getGeneralPluginsCount() == 0)
  {
    gLog.warning(tr("No plugins specified on the command-line (-p option).\n"
                    "See the README for more information."));
    return 0;
  }

  // Setup file descriptors to manage
  myMainLoop.addRawFile(myPipe.getReadFd(), this);

#ifdef USE_FIFO
  // Init the fifo
  gFifo.initialize(myMainLoop);
#endif

  // Run the plugins
  gPluginManager.startAllPlugins();

  // Reset to requested log level
  myConsoleLog->setLogLevelsFromBitmask(
      Licq::LogUtils::convertOldBitmaskToNew(myConsoleLogLevel));

  // Logon all protocols according to owner configuration
  gDaemon.autoLogon();

  // Flush statistics data regulary
  myMainLoop.addTimeout(60*1000, this, 1, false);

  // Run
  myMainLoop.run();

  gPluginManager.cancelAllPlugins();

  gDaemon.Shutdown();

  gUserManager.shutdown();

  // Flush statistics counters
  gStatistics.flush();

  return gPluginManager.getGeneralPluginsCount();
}


void CLicq::PrintUsage()
{
  printf(tr("%s version %s.\n"
         "Usage:  Licq [-h] [-d #] [-b configdir] [-I] [-p plugin] [-l protoplugin] [-o file] [-- <plugin #1 parameters>] [-- <plugin #2 parameters>...]\n\n"
         " -h : this help screen (and any plugin help screens as well)\n"
         " -d : set what information is logged to standard output:\n"
         "        1  status information\n"
         "        2  unknown packets\n"
         "        4  errors\n"
         "        8  warnings\n"
         "       16  all packets\n"
         "      add values together for multiple options\n"
         " -c : disable color at standard output\n"
         " -b : set the base directory for the config and data files (~/.licq by default)\n"
         " -I : force initialization of the given base directory\n"
         " -p : load the given plugin library\n"
         " -l : load the given protocol plugin library\n"
         " -o : redirect stderr to <file>, which can be a device (ie /dev/ttyp4)\n"),
         PACKAGE, LICQ_VERSION_STRING);
}


void CLicq::SaveLoadedPlugins()
{
  char szKey[20];

  Licq::IniFile licqConf("licq.conf");
  licqConf.loadFile();

  licqConf.setSection("plugins");

  Licq::GeneralPluginsList general;
  gPluginManager.getGeneralPluginsList(general);

  licqConf.set("NumPlugins", general.size());

  unsigned short i = 1;
  BOOST_FOREACH(GeneralPlugin::Ptr plugin, general)
  {
    sprintf(szKey, "Plugin%d", i++);
    licqConf.set(szKey, plugin->libraryName());
  }

  // Don't leave old higher numbered plugins in config
  do
  {
    sprintf(szKey, "Plugin%d", i++);
  }
  while (licqConf.unset(szKey));

  Licq::ProtocolPluginsList protocols;
  gPluginManager.getProtocolPluginsList(protocols);

  licqConf.set("NumProtoPlugins", protocols.size());

  i = 1;
  BOOST_FOREACH(ProtocolPlugin::Ptr plugin, protocols)
  {
    sprintf(szKey, "ProtoPlugin%d", i++);
    licqConf.set(szKey, plugin->libraryName());
  }

  // Don't leave old higher numbered protocols in config
  do
  {
    sprintf(szKey, "ProtoPlugin%d", i++);
  }
  while (licqConf.unset(szKey));

  licqConf.writeFile();
}


void CLicq::shutdown()
{
  // Save plugins
  if (gPluginManager.getGeneralPluginsCount() > 0)
    SaveLoadedPlugins();

  gPluginManager.shutdownAllPlugins();

  notify(NotifyShuttingDown);
}


bool CLicq::Install()
{
  string baseDir = gDaemon.baseDir();

  // Create the directory if necessary
  if (mkdir(baseDir.c_str(), 0700) == -1 && errno != EEXIST)
  {
    fprintf(stderr, "Couldn't mkdir %s: %s\n", baseDir.c_str(), strerror(errno));
    return (false);
  }
  string cmd = baseDir + "users";
  if (mkdir(cmd.c_str(), 0700) == -1 && errno != EEXIST)
  {
    fprintf(stderr, "Couldn't mkdir %s: %s\n", cmd.c_str(), strerror(errno));
    return (false);
  }

  // Create licq.conf
  Licq::IniFile licqConf("licq.conf");
  licqConf.loadFile();
  licqConf.setSection("licq");
  licqConf.set("Version", LICQ_VERSION);
  licqConf.setSection("plugins");
  licqConf.set(string("NumPlugins"), 0);
  licqConf.set(string("NumProtoPlugins"), 0);
  licqConf.setSection("network");
  licqConf.setSection("onevent");
  licqConf.setSection("groups");
  licqConf.set(string("NumOfGroups"), 0);
  licqConf.setSection("owners");
  licqConf.set(string("NumOfOwners"), 0);
  licqConf.writeFile();

  // Create users.conf
  Licq::IniFile usersConf("users.conf");
  usersConf.loadFile();
  usersConf.setSection("users");
  usersConf.set(string("NumOfUsers"), 0);
  usersConf.writeFile();

  return(true);
}
