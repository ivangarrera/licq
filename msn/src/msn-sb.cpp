/*
 * This file is part of Licq, an instant messaging client for UNIX.
 * Copyright (C) 2004-2012 Licq developers <licq-dev@googlegroups.com>
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

#include "msn.h"
#include "msnpacket.h"
#include <licq/logging/log.h>

#include <boost/foreach.hpp>
#include <string>
#include <list>
#include <vector>

#include <licq/contactlist/user.h>
#include <licq/contactlist/usermanager.h>
#include <licq/conversation.h>
#include <licq/daemon.h>
#include <licq/event.h>
#include <licq/oneventmanager.h>
#include <licq/plugin/pluginmanager.h>
#include <licq/pluginsignal.h>
#include <licq/socket.h>
#include <licq/statistics.h>
#include <licq/translator.h>
#include <licq/userevents.h>

#include "user.h"

using namespace std;
using namespace LicqMsn;
using Licq::UserId;
using Licq::Conversation;
using Licq::OnEventData;
using Licq::gConvoManager;
using Licq::gLog;
using Licq::gOnEventManager;
using Licq::gUserManager;


void CMSN::ProcessSBPacket(const Licq::UserId& socketUserId, CMSNBuffer *packet, int nSock)
{
  CMSNPacket *pReply;
  bool bSkipPacket;

  //while (!packet->End())
  {
    pReply = 0;
    bSkipPacket = true;
    string strCmd = packet->unpackRawString(3);

    if (strCmd == "IRO")
    {
      packet->SkipParameter(); // Seq
      packet->SkipParameter(); // current user to add
      packet->SkipParameter(); // total users in conversation
      UserId userId(packet->GetParameter(), MSN_PPID);

      bool newUser;
      {
        Licq::UserWriteGuard u(userId, true, &newUser);
        if (newUser)
        {
          // MSN uses UTF-8 so we need to set this for all new users automatically
          u->SetEnableSave(false);
          u->setUserEncoding("UTF-8");
          u->SetEnableSave(true);
          u->save(Licq::User::SaveLicqInfo);
        }
      }

      // Add the user to the conversation
      Conversation* convo = gConvoManager.getFromSocket(nSock);
      if (convo == NULL)
        convo = gConvoManager.add(myOwnerId, nSock);
      convo->addUser(userId);

      // Notify the plugins of the new CID
      Licq::gPluginManager.pushPluginSignal(new Licq::PluginSignal(
          Licq::PluginSignal::SignalConversation,
          Licq::PluginSignal::ConvoCreate, userId, 0, SocketToCID(nSock)));

      Licq::gPluginManager.pushPluginSignal(new Licq::PluginSignal(
          Licq::PluginSignal::SignalConversation,
          Licq::PluginSignal::ConvoJoin, userId, 0, SocketToCID(nSock)));

      gLog.info("%s joined the conversation", userId.toString().c_str());
    }
    else if (strCmd == "ANS")
    {
      // Send our capabilities
      Send_SB_Packet(Licq::UserId(), new CPS_MsnClientCaps(), nSock);
    }
    else if (strCmd == "MSG")
    {
      Licq::UserId userId(packet->GetParameter(), MSN_PPID);
      packet->SkipParameter(); // Nick
      string strSize = packet->GetParameter(); // Size
      int nSize = atoi(strSize.c_str()) + 1; // Make up for the \n
      unsigned long nBeforeParse = packet->getDataPosRead() - packet->getDataStart();
      packet->SkipPacket(); // Skip \r\n
      packet->ParseHeaders();
      unsigned long nAfterParse = packet->getDataPosRead() - packet->getDataStart();
      int nRead = nAfterParse - nBeforeParse;
      nSize -= nRead;

      string strType = packet->GetValue("Content-Type");
      if (strType == "text/x-msmsgscontrol")
      {
        packet->SkipRN();
        packet->SkipRN();
        packet->SkipRN();
        Licq::UserWriteGuard u(userId);
        if (u.isLocked())
        {
          u->setIsTyping(true);
          Licq::gPluginManager.pushPluginSignal(new Licq::PluginSignal(
              Licq::PluginSignal::SignalUser,
              Licq::PluginSignal::UserTyping, u->id(), SocketToCID(nSock)));
        }
      }
      else if (strncmp(strType.c_str(), "text/plain", 10) == 0)
      {
        gLog.info("Message from %s", userId.accountId().c_str());

        bSkipPacket = false;  
        string msg = Licq::gTranslator.returnToUnix(packet->unpackRawString(nSize));

        Licq::EventMsg* e = new Licq::EventMsg(msg, time(0), 0, SocketToCID(nSock));
        Licq::UserWriteGuard u(userId);
        if (u.isLocked())
          u->setIsTyping(false);
        if (Licq::gDaemon.addUserEvent(*u, e))
          gOnEventManager.performOnEvent(OnEventData::OnEventMessage, *u);
      }
      else if (strncmp(strType.c_str(), "text/x-msmsgsinvite", 19) == 0)
      {
        packet->SkipRN();
        packet->ParseHeaders();

        string application = packet->GetValue("Application-Name");
        string cookie = packet->GetValue("Invitation-Cookie");
        string command = packet->GetValue("Invitation-Command");

        if (command == "INVITE")
        {
          // Invitation for unknown application, tell inviter that we don't have it
          gLog.info("Invitation from %s for unknown application (%s)",
              userId.accountId().c_str(), application.c_str());
          pReply = new CPS_MSNCancelInvite(cookie, "REJECT_NOT_INSTALLED");
        }
      }
      else if (strncmp(strType.c_str(), "application/x-msnmsgrp2p", 24) == 0)
      {
	// Get the binary header
	/*
	  unsigned long nSessionID = packet->UnpackUnsignedLong();
	unsigned long nIdentifier = packet->UnpackUnsignedLong();
	unsigned long nOffset[2], nSize[2], nAckDataSize[2];
	nOffset[0] = packet->UnpackUnsignedLong();
	nOffset[1] = packet->UnpackUnsignedLong();
	nSize[0] = packet->UnpackUnsignedLong();
	nSize[1] = packet->UnpackUnsignedLong();
	unsigned long nLen = packet->UnpackUnsignedLong();
	unsigned long nFlag = packet->UnpackUnsignedLong();
	unsigned long nAckID = packet->UnpackUnsignedLong();
	unsigned long nAckUniqueID = packet->UnpackUnsignedLong();
	nAckDataSize[0] = packet->UnpackUnsignedLong();
	nAckDataSize[1] = packet->UnpackUnsignedLong();

	printf("%ld %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld\n",
	      nSessionID, nIdentifier, nOffset[0], nOffset[1], nSize[0],
	      nSize[1], nLen, nFlag, nAckID, nAckUniqueID,
	      nAckDataSize[0], nAckDataSize[1]);
	*/

	CMSNDataEvent *p = FetchDataEvent(userId, nSock);
	if (p)
	{
	  if (p->ProcessPacket(packet) > 0)
	  {
	    RemoveDataEvent(p);
	  }
	}
      }
      else if (strncmp(strType.c_str(), "text/x-clientcaps", 17) == 0)
      {
        packet->SkipRN();
        packet->ParseHeaders();
        string userClient = packet->GetValue("Client-Name");
        if (!userClient.empty())
        {
          gLog.info("Identified user client as %s", userClient.c_str());

          Licq::UserWriteGuard u(userId);
          u->setClientInfo(userClient);
        }
      }
      else
      {
        gLog.info("Message from %s with unknown content type (%s)",
            userId.accountId().c_str(), strType.c_str());
      }
    }
    else if (strCmd == "ACK")
    {
      string strId = packet->GetParameter();
      unsigned long nSeq = (unsigned long)atoi(strId.c_str());
      Licq::Event* e = RetrieveEvent(nSeq);
      if (e)
      {
        e->m_eResult = Licq::Event::ResultAcked;
        if (e->m_pUserEvent)
        {
          Conversation* convo = gConvoManager.getFromSocket(nSock);
          if (convo != NULL)
          {
            Licq::ConversationUsers users;
            convo->getUsers(users);
            BOOST_FOREACH(const UserId& userId, users)
            {
              Licq::UserWriteGuard u(userId);
              if (!u.isLocked())
                continue;

              e->m_pUserEvent->AddToHistory(*u, false);
              u->SetLastSentEvent();
              if (u->id() == e->userId())
                gOnEventManager.performOnEvent(OnEventData::OnEventMsgSent, *u);
            }
          }
          else
          {
            Licq::UserWriteGuard u(e->userId());
            if (u.isLocked())
            {
              e->m_pUserEvent->AddToHistory(*u, false);
              u->SetLastSentEvent();
              gOnEventManager.performOnEvent(OnEventData::OnEventMsgSent, *u);
            }
          }
          Licq::gStatistics.increase(Licq::Statistics::EventsSentCounter);
        }
	Licq::gPluginManager.pushPluginEvent(e);
      }
      else
      {
	// P2P response
	//packet->SkipRN();
      }
    }
    else if (strCmd == "USR")
    {
      SStartMessage *pStart = 0;
      pthread_mutex_lock(&mutex_StartList);
      StartList::iterator it;
      for (it = m_lStart.begin(); it != m_lStart.end(); it++)
      {
	if ((*it)->m_bConnecting == true)
	  continue;
	pStart = *it;
	break;
      }
      
      if (pStart)
      {
	pStart->m_bConnecting = true;
        pReply = new CPS_MSNCall(pStart->userId.accountId());
	pStart->m_nSeq = pReply->Sequence();
      }
      pthread_mutex_unlock(&mutex_StartList);
    }
    else if (strCmd == "JOI")
    {
      UserId userId(packet->GetParameter(), MSN_PPID);
      gLog.info("%s joined the conversation", userId.toString().c_str());

      SStartMessage *pStart = 0;
      StartList::iterator it;
      pthread_mutex_lock(&mutex_StartList);
      for (it = m_lStart.begin(); it != m_lStart.end(); it++)
      {
        if ((*it)->userId == userId) // case insensitive perhaps?
        {
          pStart = *it;
          m_lStart.erase(it);
          break;
        }
      }

      // Send our capabilities
      Send_SB_Packet(userId, new CPS_MsnClientCaps(), nSock);

      if ((pStart && pStart->m_bDataConnection == false) || pStart == 0)
      {
        // Add the user to the conversation
        Conversation* convo = gConvoManager.getFromSocket(nSock);
        if (convo == NULL)
          convo = gConvoManager.add(myOwnerId, nSock);
        convo->addUser(userId);

        // Notify the plugins of the new CID
        Licq::gPluginManager.pushPluginSignal(new Licq::PluginSignal(
            Licq::PluginSignal::SignalConversation,
            Licq::PluginSignal::ConvoCreate, userId, 0, SocketToCID(nSock)));

        // Notify the plugins
        Licq::gPluginManager.pushPluginSignal(new Licq::PluginSignal(
            Licq::PluginSignal::SignalConversation,
            Licq::PluginSignal::ConvoJoin, userId, 0, SocketToCID(nSock)));
      }

      if (pStart)
      {
        if (pStart->m_pEvent)
          m_pEvents.push_back(pStart->m_pEvent);

        Send_SB_Packet(pStart->userId, pStart->m_pPacket, nSock, false);

        delete pStart;
      }
      pthread_mutex_unlock(&mutex_StartList);
    }
    else if (strCmd == "BYE")
    {
      // closed the window and connection
      UserId userId(packet->GetParameter(), MSN_PPID);
      gLog.info("Connection with %s closed", userId.toString().c_str());

      Licq::gPluginManager.pushPluginSignal(new Licq::PluginSignal(
          Licq::PluginSignal::SignalConversation,
          Licq::PluginSignal::ConvoLeave, userId, 0, SocketToCID(nSock)));

      Conversation* convo = gConvoManager.getFromSocket(nSock);
      if (convo != NULL)
        convo->removeUser(userId);

      int nThisSock = -1;

      {
        UserWriteGuard u(userId);
        if (u.isLocked())
        {
          u->clearNormalSocketDesc();
          nThisSock = u->normalSocketDesc();
        }
      }

      if (convo == NULL || convo->isEmpty())
      {
        Licq::INetSocket* s = gSocketMan.FetchSocket(nThisSock);
        gSocketMan.DropSocket(s);
        gSocketMan.CloseSocket(nSock);
        if (convo != NULL)
          gConvoManager.remove(convo->id());
      }
    }
    else if (strCmd == "217")
    {
      unsigned long nSeq = packet->GetParameterUnsignedLong();

      // Search pStart for this sequence, mark it as an error, send the
      // signals to the daemon and remove these item from the list.
      SStartMessage *pStart = 0;
      StartList::iterator it;
      pthread_mutex_lock(&mutex_StartList);
      for (it = m_lStart.begin(); it != m_lStart.end(); it++)
      {
        if ((*it)->m_nSeq == nSeq)
        {
          gLog.error("User not online");
          pStart = *it;
          pStart->m_pEvent->m_eResult = Licq::Event::ResultFailed;
          Licq::gPluginManager.pushPluginEvent(pStart->m_pEvent);
          m_lStart.erase(it);
          break; 
        }
      }     
      pthread_mutex_unlock(&mutex_StartList);
    }
    else
    {
      gLog.warning("Unhandled command (%s)", strCmd.c_str());
    }
  
    // Get the next packet
    if (bSkipPacket)
      packet->SkipPacket();
    
    if (pReply)
    {
      Send_SB_Packet(socketUserId, pReply, nSock);
    }
  }
  
  //delete packet;
}

void CMSN::Send_SB_Packet(const UserId& userId, CMSNPacket *p, int nSocket, bool bDelete)
{
  int nSock = nSocket;
  if (nSocket == -1)
  {
    UserReadGuard u(userId);
    if (!u.isLocked())
      return;
    nSock = u->normalSocketDesc();
  }
  Licq::INetSocket* s = gSocketMan.FetchSocket(nSock);
  if (!s)
    s = gSocketMan.FetchSocket(nSocket);
  if (!s) return;
  Licq::TCPSocket* sock = dynamic_cast<Licq::TCPSocket*>(s);
  if (!sock->send(*p->getBuffer()) && userId.isValid())
  {
    gLog.info("Connection with %s lost", userId.toString().c_str());

    Licq::gPluginManager.pushPluginSignal(new Licq::PluginSignal(
        Licq::PluginSignal::SignalConversation,
        Licq::PluginSignal::ConvoLeave, userId, 0, SocketToCID(nSock)));

    Conversation* convo = gConvoManager.getFromSocket(nSock);
    if (convo != NULL)
      convo->removeUser(userId);

    {
      UserWriteGuard u(userId);
      if (u.isLocked())
        u->clearNormalSocketDesc();
    }

    if (convo == NULL || convo->isEmpty())
    {
      gSocketMan.DropSocket(s);
      gSocketMan.CloseSocket(nSock);
      if (convo != NULL)
        gConvoManager.remove(convo->id());
    }
  }
  else
    gSocketMan.DropSocket(sock);
  
  if (bDelete)
    delete p;
}

bool CMSN::MSNSBConnectStart(const string &strServer, const string &strCookie)
{
  size_t sep = strServer.rfind(':');
  string host;
  int port;
  if (sep != string::npos)
  {
    host = strServer.substr(0, sep);
    port = atoi(strServer.substr(sep+1).c_str());
  }
  else
  {
    gLog.info("Connecting to SB at %s failed, invalid address",
        strServer.c_str());
    return false;
  }

  SStartMessage *pStart = 0;  
  pthread_mutex_lock(&mutex_StartList);
  StartList::iterator it;
  for (it = m_lStart.begin(); it != m_lStart.end(); it++)
  {
    if ((*it)->m_bConnecting == true)
      continue;
    pStart = *it;
    break;
  }
  if (!pStart)
  {
    pthread_mutex_unlock(&mutex_StartList);
    return false;
  }
  //pStart->m_bConnecting = true;
  Licq::TCPSocket* sock = new Licq::TCPSocket(pStart->userId);
  pthread_mutex_unlock(&mutex_StartList);

  gLog.info("Connecting to SB at %s:%d", host.c_str(), port);
  if (!sock->connectTo(host, port))
  {
    gLog.error("Connection to SB at %s failed", host.c_str());
    delete sock;
    return false;
  }

  int nSocket = sock->Descriptor();

  // This socket was just opened so make sure there isn't any old left over conversation associated with it
  killConversation(nSocket);

  gSocketMan.AddSocket(sock);

  {
    UserWriteGuard u(pStart->userId);
    if (u.isLocked())
    {
      if (pStart->m_bDataConnection)
        u->setInfoSocketDesc(sock);
      else
        u->setNormalSocketDesc(sock);
    }
  }
  gSocketMan.DropSocket(sock);

  CMSNPacket* pReply = new CPS_MSN_SBStart(strCookie, myOwnerId.accountId());
  Send_SB_Packet(pStart->userId, pReply, nSocket);

  return true;
}

bool CMSN::MSNSBConnectAnswer(const string& strServer, const string& strSessionId,
    const string& strCookie, const Licq::UserId& userId)
{
  size_t sep = strServer.rfind(':');
  string host;
  int port;
  if (sep != string::npos)
  {
    host = strServer.substr(0, sep);
    port = atoi(strServer.substr(sep+1).c_str());
  }
  else
  {
    gLog.info("Connecting to SB at %s failed, invalid address",
        strServer.c_str());
    return false;
  }

  Licq::TCPSocket* sock = new Licq::TCPSocket(userId);
  gLog.info("Connecting to SB at %s:%d", host.c_str(), port);
  if (!sock->connectTo(host, port))
  {
    gLog.error("Connection to SB at %s failed", host.c_str());
    delete sock;
    return false;
  }
  int nSocket = sock->Descriptor();

  // This socket was just opened so make sure there isn't any old left over conversation associated with it
  killConversation(nSocket);

  gSocketMan.AddSocket(sock);
  CMSNPacket* pReply = new CPS_MSN_SBAnswer(strSessionId, strCookie, myOwnerId.accountId());

  {
    bool newUser = false;
    UserWriteGuard u(userId, true, &newUser);
    u->setNormalSocketDesc(sock);
    if (newUser)
    {
      u->SetEnableSave(false);
      u->setUserEncoding("UTF-8");
      u->SetEnableSave(true);
      u->save(Licq::User::SaveLicqInfo);
    }
  }

  gSocketMan.DropSocket(sock);

  Send_SB_Packet(userId, pReply, nSocket);

  return true;
}

void CMSN::MSNSendInvitation(const Licq::UserId& userId, CMSNPacket* _pPacket)
{
  // Must connect to the SB and call the user
  CMSNPacket *pSB = new CPS_MSNXfr();
      
  SStartMessage *p = new SStartMessage;
  p->m_pPacket = _pPacket;
  p->m_pEvent = 0;
  p->userId = userId;
  p->m_nSeq = pSB->Sequence();
  p->m_bConnecting = false;
  p->m_bDataConnection = true;
  pthread_mutex_lock(&mutex_StartList);
  m_lStart.push_back(p);
  pthread_mutex_unlock(&mutex_StartList);
  
  SendPacket(pSB);
}

void CMSN::MSNSendMessage(unsigned long eventId, const UserId& userId, const string& message,
    pthread_t _tPlugin, unsigned long _nCID)
{
  int nSocket = -1;
  
  if (_nCID)
  {
    Conversation* convo = gConvoManager.get(_nCID);
    if (convo != NULL)
      nSocket = convo->socketId();
  }

  string msgDos = Licq::gTranslator.returnToDos(message);
  CMSNPacket* pSend = new CPS_MSNMessage(msgDos.c_str());
  Licq::EventMsg* m = new Licq::EventMsg(message, Licq::UserEvent::TimeNow, Licq::EventMsg::FlagSender);
  Licq::Event* e = new Licq::Event(_tPlugin, eventId, 0, pSend, Licq::Event::ConnectServer, userId, m);
  e->myCommand = Licq::Event::CommandMessage;

  if (nSocket > 0)
  {
    m_pEvents.push_back(e);

    Send_SB_Packet(userId, pSend, nSocket, false);
  }
  else
  {
    // Must connect to the SB and call the user
    CMSNPacket *pSB = new CPS_MSNXfr();
      
    SStartMessage *p = new SStartMessage;
    p->m_pPacket = pSend;
    p->m_pEvent = e;
    p->userId = userId;
    p->m_nSeq = pSB->Sequence();
    p->m_bConnecting = false;
    p->m_bDataConnection = false;
    pthread_mutex_lock(&mutex_StartList);
    m_lStart.push_back(p);
    pthread_mutex_unlock(&mutex_StartList);
   
    SendPacket(pSB);
  }  
}

void CMSN::MSNSendTypingNotification(const UserId& userId, unsigned long _nCID)
{
  CMSNPacket* pSend = new CPS_MSNTypingNotification(myOwnerId.accountId());
  int nSockDesc = -1;
  
  if (_nCID)
  {
    Conversation* convo = gConvoManager.get(_nCID);
    if (convo != NULL)
      nSockDesc = convo->socketId();
  }

  if (nSockDesc > 0)
    Send_SB_Packet(userId, pSend, nSockDesc);
}

void CMSN::killConversation(int sock)
{
  Conversation* convo;
  // There should never be more than one but loop just in case
  while ((convo = gConvoManager.getFromSocket(sock)) != NULL)
  {
    int convoId = convo->id();

    // Get all users from the conversation and disassociate them
    Licq::ConversationUsers users;
    convo->getUsers(users);
    BOOST_FOREACH(const UserId& userId, users)
    {
      // Signal that user is removed
      Licq::gPluginManager.pushPluginSignal(new Licq::PluginSignal(
          Licq::PluginSignal::SignalConversation,
          Licq::PluginSignal::ConvoLeave, userId, 0, convoId));

      // Remove user from the conversation
      convo->removeUser(userId);

      // Clear socket from user if it's still is associated with this conversation
      UserWriteGuard u(userId);
      if (u.isLocked() && u->normalSocketDesc() == sock)
        u->clearNormalSocketDesc();
    }
    gConvoManager.remove(convoId);
  }
}
