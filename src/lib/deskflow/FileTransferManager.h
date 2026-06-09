/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2024 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "deskflow/FileReceiver.h"
#include "deskflow/FileSender.h"

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace deskflow {
class IStream;
}

class IEventQueue;

using FileTransferRequestCallback = std::function<bool(
    uint32_t transferId, const std::string &sourceScreen, uint32_t fileCount, uint64_t totalSize
)>;

class FileTransferManager
{
public:
  static FileTransferManager &instance();

  void init(IEventQueue *events, const std::string &downloadDir);

  uint32_t sendFiles(
      const std::vector<std::string> &files, const std::string &baseDir, deskflow::IStream *stream, void *eventTarget
  );

  void handleDragInfo(
      uint32_t transferId, uint32_t fileCount, uint64_t totalSize, const std::string &sourceScreen,
      deskflow::IStream *stream, void *eventTarget
  );

  bool handleFileChunk(uint32_t transferId, uint8_t mark, const std::string &data);
  bool acceptTransfer(uint32_t transferId);
  void rejectTransfer(uint32_t transferId, const std::string &reason = "Rejected by user");
  void cancelTransfer(uint32_t transferId, const std::string &reason = "Cancelled by user");
  bool isTransferActive(uint32_t transferId) const;
  uint32_t nextTransferId();

  void setDownloadDir(const std::string &dir);

  const std::string &downloadDir() const
  {
    return m_downloadDir;
  }

  void setRequestCallback(FileTransferRequestCallback callback)
  {
    m_requestCallback = std::move(callback);
  }

  void processEvents();

private:
  FileTransferManager() = default;
  ~FileTransferManager() = default;

  FileTransferManager(const FileTransferManager &) = delete;
  FileTransferManager &operator=(const FileTransferManager &) = delete;

  IEventQueue *m_events = nullptr;
  std::string m_downloadDir;

  std::map<uint32_t, std::unique_ptr<FileSender>> m_senders;
  std::map<uint32_t, std::unique_ptr<FileReceiver>> m_receivers;

  struct PendingTransfer
  {
    uint32_t transferId;
    uint32_t fileCount;
    uint64_t totalSize;
    std::string sourceScreen;
    deskflow::IStream *stream;
    void *eventTarget;
  };
  std::map<uint32_t, PendingTransfer> m_pendingTransfers;

  uint32_t m_nextTransferId = 1;
  FileTransferRequestCallback m_requestCallback;
  mutable std::mutex m_mutex;
};
