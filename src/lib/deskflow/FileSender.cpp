/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2024 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "deskflow/FileSender.h"

#include "base/Event.h"
#include "base/IEventQueue.h"
#include "base/Log.h"
#include "deskflow/FileTransferChunk.h"
#include "deskflow/ProtocolTypes.h"

#include <fstream>

FileSender::FileSender(uint32_t transferId, IEventQueue *events, void *eventTarget)
    : m_transferId(transferId),
      m_events(events),
      m_eventTarget(eventTarget)
{
}

void FileSender::queueFiles(
    const std::vector<std::filesystem::path> &paths, const std::filesystem::path &baseDir
)
{
  for (const auto &path : paths) {
    collectFiles(path, baseDir);
  }
  m_totalFiles = static_cast<uint32_t>(m_fileQueue.size());
}

void FileSender::collectFiles(const std::filesystem::path &path, const std::filesystem::path &baseDir)
{
  std::error_code ec;
  if (!std::filesystem::exists(path, ec)) {
    LOG_WARN("file transfer: path does not exist: %s", path.c_str());
    return;
  }

  FileTransferInfo info;
  info.absolutePath = path;
  info.relativePath = std::filesystem::relative(path, baseDir, ec).string();
  if (ec) {
    LOG_WARN("file transfer: cannot compute relative path for %s: %s", path.c_str(), ec.message().c_str());
    info.relativePath = path.filename().string();
  }

  if (std::filesystem::is_directory(path, ec)) {
    info.isDirectory = true;
    info.size = 0;
    m_fileQueue.push(info);

    // Recursively collect files in directory
    for (const auto &entry : std::filesystem::directory_iterator(path, ec)) {
      collectFiles(entry.path(), baseDir);
    }
  } else {
    info.isDirectory = false;
    info.size = std::filesystem::file_size(path, ec);
    if (ec) {
      LOG_WARN("file transfer: cannot get file size for %s: %s", path.c_str(), ec.message().c_str());
      info.size = 0;
    }
    m_totalBytes += info.size;
    m_fileQueue.push(info);
  }
}

void FileSender::start()
{
  if (m_started) {
    return;
  }
  m_started = true;

  sendDragInfo();

  // Start sending the first file
  if (!openNextFile()) {
    // No files to send, send transfer end
    auto *chunk = FileTransferChunk::transferEnd(m_transferId);
    m_events->addEvent(Event(EventTypes::FileTransferSending, m_eventTarget, chunk));
    m_complete = true;

    if (m_completeCallback) {
      m_completeCallback(true, "");
    }
  }
}

bool FileSender::openNextFile()
{
  // Close current file if open
  if (m_currentFile.is_open()) {
    m_currentFile.close();
  }

  while (!m_fileQueue.empty()) {
    m_currentFileInfo = m_fileQueue.front();
    m_fileQueue.pop();

    if (m_currentFileInfo.isDirectory) {
      // Directories don't need data transfer, just continue
      continue;
    }

    m_currentFile.open(m_currentFileInfo.absolutePath, std::ios::binary);
    if (!m_currentFile.is_open()) {
      LOG_ERR("file transfer: cannot open file: %s", m_currentFileInfo.absolutePath.c_str());
      continue;
    }

    m_currentFileBytesSent = 0;

    // Send file start chunk
    auto *chunk = FileTransferChunk::fileStart(m_transferId, m_currentFileInfo.relativePath);
    m_events->addEvent(Event(EventTypes::FileTransferSending, m_eventTarget, chunk));

    LOG_DEBUG("file transfer: starting file '%s' (%zu bytes)", m_currentFileInfo.relativePath.c_str(), m_currentFileInfo.size);
    return true;
  }

  return false;
}

bool FileSender::sendNextChunk()
{
  if (m_complete) {
    return false;
  }

  if (!m_currentFile.is_open()) {
    // Try to open next file
    if (!openNextFile()) {
      // No more files, send transfer end
      auto *chunk = FileTransferChunk::transferEnd(m_transferId);
      m_events->addEvent(Event(EventTypes::FileTransferSending, m_eventTarget, chunk));
      m_complete = true;

      if (m_completeCallback) {
        m_completeCallback(true, "");
      }
      return false;
    }
  }

  // Read chunk from file
  char buffer[kChunkSize];
  m_currentFile.read(buffer, kChunkSize);
  size_t bytesRead = static_cast<size_t>(m_currentFile.gcount());

  if (bytesRead > 0) {
    // Send data chunk
    std::string data(buffer, bytesRead);
    auto *chunk = FileTransferChunk::fileData(m_transferId, data);
    m_events->addEvent(Event(EventTypes::FileTransferSending, m_eventTarget, chunk));

    m_currentFileBytesSent += bytesRead;
    m_bytesSent += bytesRead;

    // Report progress
    if (m_progressCallback) {
      m_progressCallback(m_currentFileBytesSent, m_currentFileInfo.size, m_filesCompleted, m_totalFiles);
    }
  }

  // Check if file is complete
  if (m_currentFile.eof() || m_currentFileBytesSent >= m_currentFileInfo.size) {
    // Send file end chunk
    auto *chunk = FileTransferChunk::fileEnd(m_transferId);
    m_events->addEvent(Event(EventTypes::FileTransferSending, m_eventTarget, chunk));

    m_filesCompleted++;
    m_currentFile.close();

    LOG_DEBUG("file transfer: completed file '%s'", m_currentFileInfo.relativePath.c_str());

    // Try next file
    if (!openNextFile()) {
      // No more files, send transfer end
      auto *endChunk = FileTransferChunk::transferEnd(m_transferId);
      m_events->addEvent(Event(EventTypes::FileTransferSending, m_eventTarget, endChunk));
      m_complete = true;

      if (m_completeCallback) {
        m_completeCallback(true, "");
      }
      return false;
    }
  }

  return true;
}

void FileSender::cancel(const std::string &reason)
{
  if (m_complete) {
    return;
  }

  if (m_currentFile.is_open()) {
    m_currentFile.close();
  }

  auto *chunk = FileTransferChunk::transferCancel(m_transferId, reason);
  m_events->addEvent(Event(EventTypes::FileTransferSending, m_eventTarget, chunk));

  m_complete = true;

  if (m_completeCallback) {
    m_completeCallback(false, reason);
  }
}

void FileSender::sendDragInfo()
{
  // Format: "transferId:fileCount:totalSize"
  std::string infoStr = std::to_string(m_transferId) + ":" + std::to_string(m_totalFiles) + ":" + std::to_string(m_totalBytes);

  LOG_DEBUG("file transfer: drag info ready, %d files, %llu bytes", m_totalFiles, m_totalBytes);
}
