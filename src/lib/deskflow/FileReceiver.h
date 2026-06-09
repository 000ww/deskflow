/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2024 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <cstdint>
#include <functional>
#include <string>

class QFile;

namespace deskflow {
class IStream;
}

enum class FileReceiverState
{
  Waiting,
  ReceivingFile,
  Complete,
  Error
};

using FileReceiveProgressCallback = std::function<void(
    uint64_t bytesReceived, uint64_t totalBytes, const std::string &currentFile
)>;

using FileReceiveCompleteCallback = std::function<void(bool success, const std::string &message)>;

class FileReceiver
{
public:
  FileReceiver(uint32_t transferId, const std::string &downloadDir);
  ~FileReceiver();

  bool onFileStart(const std::string &relativePath);
  bool onDataChunk(const std::string &data);
  bool onFileEnd();
  void onTransferEnd();
  void onTransferCancel(const std::string &reason);

  FileReceiverState state() const
  {
    return m_state;
  }

  uint32_t transferId() const
  {
    return m_transferId;
  }

  void setProgressCallback(FileReceiveProgressCallback callback)
  {
    m_progressCallback = std::move(callback);
  }

  void setCompleteCallback(FileReceiveCompleteCallback callback)
  {
    m_completeCallback = std::move(callback);
  }

  const std::string &downloadDir() const
  {
    return m_downloadDir;
  }

private:
  std::string sanitizePath(const std::string &path);
  bool ensureParentDirectories(const std::string &path);

  uint32_t m_transferId;
  std::string m_downloadDir;

  FileReceiverState m_state = FileReceiverState::Waiting;

  void *m_currentFile = nullptr;
  std::string m_currentFilePath;
  std::string m_tempFilePath;

  uint64_t m_currentFileBytesReceived = 0;
  uint64_t m_totalBytesReceived = 0;
  uint32_t m_filesReceived = 0;

  FileReceiveProgressCallback m_progressCallback;
  FileReceiveCompleteCallback m_completeCallback;
};
