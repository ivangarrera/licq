#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "sar.h"
#include "log.h"
#include "sar.conf.h"
#include "constants.h"

CSARManager gSARManager;



/*---------------------------------------------------------------------------
 * CSavedAutoResponse
 *-------------------------------------------------------------------------*/
CSavedAutoResponse::CSavedAutoResponse(const char *_szName, const char *_szAutoResponse)
{
  m_szName = strdup(_szName);
  m_szAutoResponse = strdup(_szAutoResponse);
}

/*---------------------------------------------------------------------------
 * ~CSavedAutoResponse
 *-------------------------------------------------------------------------*/
CSavedAutoResponse::~CSavedAutoResponse(void)
{
  free(m_szName);
  free(m_szAutoResponse);
}


/*---------------------------------------------------------------------------
 * CSARManager
 *-------------------------------------------------------------------------*/
CSARManager::CSARManager(void)
{
}


/*---------------------------------------------------------------------------
 * ~CSARManager
 *-------------------------------------------------------------------------*/
CSARManager::~CSARManager(void)
{
  for (unsigned short i = 0; i < SAR_NUM_SECTIONS; i++)
    for (SARListIter iter = m_lSAR[i].begin(); iter != m_lSAR[i].end(); iter++)
      delete (*iter);
}


/*---------------------------------------------------------------------------
 * CSARManager::Load
 *-------------------------------------------------------------------------*/
bool CSARManager::Load(void)
{
  char filename[128], szTemp1[32], szTemp2[32], szTemp3[32];
  unsigned short nTemp;
  char *n[] = SAR_SECTIONS;

  gLog.Info("%sLoading saved auto-responses.\n", L_INITxSTR);

  // Get data from the config file
  sprintf(filename, "%s%s", BASE_DIR, "sar.conf");
  // Try and load the file
  if (!m_fConf.LoadFile(filename))
  {
    gLog.Warn("%sUnable to open SAR config file \"%s\", creating default.\n",
              L_WARNxSTR, filename);
    // Create sar.conf
    FILE *f = fopen(filename, "w");
    fprintf(f, "%s", SAR_CONF);
    fclose(f);
    if (!m_fConf.ReloadFile()) return false;
  }

  // Read in auto response information
  for (unsigned short i = 0; i < SAR_NUM_SECTIONS; i++)
  {
    m_fConf.SetSection(n[i]);
    m_fConf.ReadNum("NumSAR", nTemp, 0);
    for (unsigned short i = 0; i < nTemp; i++)
    {
      sprintf(szTemp1, "SAR%d.Name", i + 1);
      m_fConf.ReadStr(szTemp1, szTemp2, "");
      sprintf(szTemp1, "SAR%d.Text", i + 1);
      m_fConf.ReadStr(szTemp1, szTemp3, "");
      m_lSAR[i].push_back(new CSavedAutoResponse(szTemp2, szTemp3));
    }
  }

  return true;
}


/*---------------------------------------------------------------------------
 * CSARManager::Save
 *-------------------------------------------------------------------------*/
void CSARManager::Save(void)
{
  char *n[] = SAR_SECTIONS;
  char sz[32];
  for (unsigned short i = 0; i < SAR_NUM_SECTIONS; i ++)
  {
    m_fConf.SetSection(n[i]);
    m_fConf.WriteNum("NumSAR", (unsigned short)m_lSAR[i].size());
    unsigned short j = 1;
    for (SARListIter iter = m_lSAR[i].begin(); iter != m_lSAR[i].end(); iter++, j++)
    {
      sprintf(sz, "SAR%d.Name", j);
      m_fConf.WriteStr(sz, (*iter)->Name());
      sprintf(sz, "SAR%d.Text", j);
      m_fConf.WriteStr(sz, (*iter)->AutoResponse());
    }
  }
}


/*---------------------------------------------------------------------------
 * SARManager::SavedAutoResponses
 *-------------------------------------------------------------------------*/
SARList &CSARManager::SavedAutoResponses(unsigned short n)
{
  if (n >= SAR_NUM_SECTIONS) n = 0;
  return m_lSAR[n];
}


