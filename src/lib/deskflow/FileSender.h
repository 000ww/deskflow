/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2024 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <cstdint>
#include <functional>
#include <queue>
#include <string>
#include <vector>

namespace deskflow {
class IStream;
}

class IEventQueue;

struct FileTransferInfo
{
  std::string absolutePath;
  std::string relativePath;
  uint64_t size = 0;
  bool isDirectory = false;
};

using FileTransferProgressCallback = std::function<void(
    uint64_t bytesTransferred, uint64_t totalBytes, uint32_t filesTransferred, uint32_t totalFiles
)>;

using FileTransferCompleteCallback = std::function<void(bool success, const std::string &message)>;

class FileSender
{
public:
  FileSender(uint32_t transferId, IEventQueue *events, void *eventTarget);
  ~FileSender() = default;

  void queueFiles(const std::vector<std::string> &paths, const std::string &baseDir);
  void start();
  bool sendNextChunk();
  void cancel(const std::string &reason);

  bool isComplete() const
  {
    return m_complete;
  }

  uint32_t transferId() const
  {
    return m_transferId;
  }

  void setProgressCallback(FileTransferProgressCallback callback)
  {
    m_progressCallback = std::move(callback);
  }

  void setCompleteCallback(FileTransferCompleteCallback callback)
  {
    m_completeCallback = std::move(callback);
  }

  uint64_t totalBytes() const
  {
    return m_totalBytes;
  }

  uint32_t totalFiles() const
  {
    return m_totalFiles;
  }

  uint64_t bytesSent() const
  {
    return m_bytesSent;
  }

  uint32_t filesCompleted() const
  {
    return m_filesCompleted;
  }

private:
  void collectFiles(const std::string &path, const std::string &baseDir);
  bool openNextFile();
  void sendDragInfo();

  uint32_t m_transferId;
  IEventQueue *m_events;
  void *m_eventTarget;

  std::queue<FileTransferInfo> m_fileQueue;

  uint64_t m_totalBytes = 0;
  uint32_t m_totalFiles = 0;
  uint64_t m_bytesSent = 0;
  uint32_t m_filesCompleted = 0;

  std::ifstream m_currentFile;
  FileTransferInfo m_currentFileInfo;
  uint64_t m_currentFileBytesSent = 0;

  bool m_complete = false;
  bool m_started = false;

  FileTransferProgressCallback m_progressCallback;
  FileTransferCompleteCallback m_completeCallback;

  static constexpr size_t kChunkSize = 512 * 1024;
};
