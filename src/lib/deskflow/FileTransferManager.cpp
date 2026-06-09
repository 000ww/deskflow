/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2024 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "deskflow/FileTransferManager.h"

#include "base/Event.h"
#include "base/IEventQueue.h"
#include "base/Log.h"
#include "deskflow/FileTransferChunk.h"
#include "deskflow/ProtocolTypes.h"

#include <filesystem>

FileTransferManager &FileTransferManager::instance()
{
  static FileTransferManager instance;
  return instance;
}

void FileTransferManager::init(IEventQueue *events, const std::string &downloadDir)
{
  std::lock_guard<std::mutex> lock(m_mutex);
  m_events = events;
  m_downloadDir = downloadDir;

  std::error_code ec;
  std::filesystem::create_directories(downloadDir, ec);
  if (ec) {
    LOG_ERR("file transfer: cannot create download directory: %s: %s", downloadDir.c_str(), ec.message().c_str());
  }

  LOG_DEBUG("file transfer: initialized, download dir=%s", downloadDir.c_str());
}

uint32_t FileTransferManager::sendFiles(
    const std::vector<std::string> &files, const std::string &baseDir, deskflow::IStream *stream, void *eventTarget
)
{
  std::lock_guard<std::mutex> lock(m_mutex);

  uint32_t transferId = m_nextTransferId++;

  auto sender = std::make_unique<FileSender>(transferId, m_events, eventTarget);
  sender->queueFiles(files, baseDir);

  if (sender->totalFiles() == 0) {
    LOG_WARN("file transfer: no files to send");
    return 0;
  }

  LOG_DEBUG(
      "file transfer: starting send, id=%d, files=%d, bytes=%llu", transferId, sender->totalFiles(), sender->totalBytes()
  );

  m_senders[transferId] = std::move(sender);
  m_senders[transferId]->start();

  return transferId;
}

void FileTransferManager::handleDragInfo(
    uint32_t transferId, uint32_t fileCount, uint64_t totalSize, const std::string &sourceScreen,
    deskflow::IStream *stream, void *eventTarget
)
{
  LOG_DEBUG(
      "file transfer: received drag info, id=%d, files=%d, bytes=%llu from %s", transferId, fileCount, totalSize,
      sourceScreen.c_str()
  );

  if (!m_requestCallback) {
    LOG_WARN("file transfer: no request callback set, rejecting transfer");
    rejectTransfer(transferId, "No request callback");
    return;
  }

  PendingTransfer pending;
  pending.transferId = transferId;
  pending.fileCount = fileCount;
  pending.totalSize = totalSize;
  pending.sourceScreen = sourceScreen;
  pending.stream = stream;
  pending.eventTarget = eventTarget;

  {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_pendingTransfers[transferId] = pending;
  }

  bool accepted = m_requestCallback(transferId, sourceScreen, fileCount, totalSize);

  if (accepted) {
    acceptTransfer(transferId);
  } else {
    rejectTransfer(transferId, "Rejected by user");
  }
}

bool FileTransferManager::handleFileChunk(uint32_t transferId, uint8_t mark, const std::string &data)
{
  std::lock_guard<std::mutex> lock(m_mutex);

  auto it = m_receivers.find(transferId);
  if (it == m_receivers.end()) {
    LOG_WARN("file transfer: received chunk for unknown transfer id=%d", transferId);
    return false;
  }

  auto *receiver = it->second.get();

  switch (mark) {
  case FileChunkType::FileStart:
    return receiver->onFileStart(data);

  case FileChunkType::FileData:
    return receiver->onDataChunk(data);

  case FileChunkType::FileEnd:
    return receiver->onFileEnd();

  case FileChunkType::TransferEnd:
    receiver->onTransferEnd();
    m_receivers.erase(it);
    return true;

  case FileChunkType::TransferCancel:
    receiver->onTransferCancel(data);
    m_receivers.erase(it);
    return true;

  default:
    LOG_ERR("file transfer: unknown chunk type %d", mark);
    return false;
  }
}

bool FileTransferManager::acceptTransfer(uint32_t transferId)
{
  std::lock_guard<std::mutex> lock(m_mutex);

  auto pendingIt = m_pendingTransfers.find(transferId);
  if (pendingIt == m_pendingTransfers.end()) {
    LOG_WARN("file transfer: no pending transfer id=%d", transferId);
    return false;
  }

  auto &pending = pendingIt->second;

  auto receiver = std::make_unique<FileReceiver>(transferId, m_downloadDir);
  m_receivers[transferId] = std::move(receiver);

  m_pendingTransfers.erase(pendingIt);

  LOG_DEBUG("file transfer: accepted transfer id=%d", transferId);
  return true;
}

void FileTransferManager::rejectTransfer(uint32_t transferId, const std::string &reason)
{
  std::lock_guard<std::mutex> lock(m_mutex);

  auto pendingIt = m_pendingTransfers.find(transferId);
  if (pendingIt != m_pendingTransfers.end()) {
    m_pendingTransfers.erase(pendingIt);
  }

  auto *chunk = FileTransferChunk::transferCancel(transferId, reason);
  m_events->addEvent(Event(EventTypes::FileTransferSending, nullptr, chunk));

  LOG_DEBUG("file transfer: rejected transfer id=%d: %s", transferId, reason.c_str());
}

void FileTransferManager::cancelTransfer(uint32_t transferId, const std::string &reason)
{
  std::lock_guard<std::mutex> lock(m_mutex);

  auto senderIt = m_senders.find(transferId);
  if (senderIt != m_senders.end()) {
    senderIt->second->cancel(reason);
    m_senders.erase(senderIt);
    return;
  }

  auto receiverIt = m_receivers.find(transferId);
  if (receiverIt != m_receivers.end()) {
    receiverIt->second->onTransferCancel(reason);
    m_receivers.erase(receiverIt);
    return;
  }

  LOG_WARN("file transfer: no active transfer id=%d to cancel", transferId);
}

bool FileTransferManager::isTransferActive(uint32_t transferId) const
{
  std::lock_guard<std::mutex> lock(m_mutex);

  return m_senders.find(transferId) != m_senders.end() || m_receivers.find(transferId) != m_receivers.end() ||
         m_pendingTransfers.find(transferId) != m_pendingTransfers.end();
}

uint32_t FileTransferManager::nextTransferId()
{
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_nextTransferId++;
}

void FileTransferManager::setDownloadDir(const std::string &dir)
{
  std::lock_guard<std::mutex> lock(m_mutex);
  m_downloadDir = dir;

  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  if (ec) {
    LOG_ERR("file transfer: cannot create download directory: %s: %s", dir.c_str(), ec.message().c_str());
  }
}

void FileTransferManager::processEvents()
{
  std::lock_guard<std::mutex> lock(m_mutex);

  for (auto it = m_senders.begin(); it != m_senders.end();) {
    auto *sender = it->second.get();

    if (sender->isComplete()) {
      it = m_senders.erase(it);
      continue;
    }

    sender->sendNextChunk();
    ++it;
  }
}
