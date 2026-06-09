/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2013 - 2016 Symless Ltd.
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "server/ClientProxy1_5.h"

#include "base/Log.h"
#include "deskflow/FileTransferChunk.h"
#include "deskflow/FileTransferManager.h"
#include "deskflow/ProtocolTypes.h"
#include "deskflow/ProtocolUtil.h"
#include "deskflow/StreamChunker.h"
#include "io/IStream.h"
#include "server/Server.h"

#include <cstring>

//
// ClientProxy1_5
//

ClientProxy1_5::ClientProxy1_5(const std::string &name, deskflow::IStream *stream, Server *server, IEventQueue *events)
    : ClientProxy1_4(name, stream, server, events)
{
}

void ClientProxy1_5::sendDragInfo(uint32_t fileCount, const char *info, size_t size)
{
  LOG_DEBUG("sending drag info to client, fileCount=%d", fileCount);

  // Parse the info to get transfer details
  // info should contain transferId and totalSize
  std::string infoStr(info, size);

  ProtocolUtil::writef(getStream(), kMsgDDragInfo, fileCount, &infoStr);
}

void ClientProxy1_5::fileChunkSending(uint8_t mark, char *data, size_t dataSize)
{
  LOG_VERBOSE("sending file chunk to client, mark=%d, size=%zu", mark, dataSize);

  std::string chunkData(data, dataSize);
  ProtocolUtil::writef(getStream(), kMsgDFileTransfer, mark, &chunkData);
}

bool ClientProxy1_5::parseMessage(const uint8_t *code)
{
  if (memcmp(code, kMsgDFileTransfer, 4) == 0) {
    fileChunkReceived();
  } else if (memcmp(code, kMsgDDragInfo, 4) == 0) {
    dragInfoReceived();
  } else {
    return ClientProxy1_4::parseMessage(code);
  }

  return true;
}

void ClientProxy1_5::fileChunkReceived() const
{
  LOG_DEBUG("received file chunk from client");

  uint32_t transferId;
  uint8_t mark;
  std::string data;

  auto state = FileTransferChunk::assemble(getStream(), transferId, mark, data);
  if (state == TransferState::Error) {
    LOG_ERR("failed to assemble file transfer chunk");
    return;
  }

  FileTransferManager::instance().handleFileChunk(transferId, mark, data);
}

void ClientProxy1_5::dragInfoReceived() const
{
  LOG_DEBUG("received drag info from client");

  uint16_t fileCount;
  std::string info;

  if (!ProtocolUtil::readf(getStream(), kMsgDDragInfo + 4, &fileCount, &info)) {
    LOG_ERR("failed to read drag info");
    return;
  }

  // Parse the info string to get transferId and totalSize
  // For now, we'll use a simple format: "transferId:totalSize"
  uint32_t transferId = 0;
  uint64_t totalSize = 0;

  size_t colonPos = info.find(':');
  if (colonPos != std::string::npos) {
    transferId = std::stoul(info.substr(0, colonPos));
    totalSize = std::stoull(info.substr(colonPos + 1));
  }

  // Get client name as source screen
  std::string sourceScreen = getName();

  FileTransferManager::instance().handleDragInfo(
      transferId, fileCount, totalSize, sourceScreen, getStream(), getEventTarget()
  );
}
