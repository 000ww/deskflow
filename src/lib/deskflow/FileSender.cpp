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

#include <QDir>
#include <QFileInfo>
#include <fstream>
#include <memory>

FileSender::FileSender(uint32_t transferId, IEventQueue *events, void *eventTarget)
    : m_transferId(transferId),
      m_events(events),
      m_eventTarget(eventTarget)
{
}

FileSender::~FileSender()
{
  auto *file = static_cast<std::ifstream *>(m_currentFile);
  delete file;
}

void FileSender::queueFiles(const std::vector<std::string> &paths, const std::string &baseDir)
{
  for (const auto &path : paths) {
    collectFiles(path, baseDir);
  }
  m_totalFiles = static_cast<uint32_t>(m_fileQueue.size());
}

void FileSender::collectFiles(const std::string &path, const std::string &baseDir)
{
  QFileInfo fi(QString::fromStdString(path));
  if (!fi.exists()) {
    LOG_WARN("file transfer: path does not exist: %s", path.c_str());
    return;
  }

  FileTransferInfo info;
  info.absolutePath = path;

  QDir base(QString::fromStdString(baseDir));
  info.relativePath = base.relativeFilePath(fi.absoluteFilePath()).toStdString();

  if (fi.isDir()) {
    info.isDirectory = true;
    info.size = 0;
    m_fileQueue.push(info);

    QDir dir(fi.absoluteFilePath());
    for (const auto &entry : dir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot)) {
      collectFiles(entry.absoluteFilePath().toStdString(), baseDir);
    }
  } else {
    info.isDirectory = false;
    info.size = static_cast<uint64_t>(fi.size());
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

  if (!openNextFile()) {
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
  auto *file = static_cast<std::ifstream *>(m_currentFile);
  if (file && file->is_open()) {
    file->close();
  }

  while (!m_fileQueue.empty()) {
    m_currentFileInfo = m_fileQueue.front();
    m_fileQueue.pop();

    if (m_currentFileInfo.isDirectory) {
      continue;
    }

    delete file;
    file = new std::ifstream(m_currentFileInfo.absolutePath, std::ios::binary);
    m_currentFile = file;

    if (!file->is_open()) {
      LOG_ERR("file transfer: cannot open file: %s", m_currentFileInfo.absolutePath.c_str());
      continue;
    }

    m_currentFileBytesSent = 0;

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

  auto *file = static_cast<std::ifstream *>(m_currentFile);
  if (!file || !file->is_open()) {
    if (!openNextFile()) {
      auto *chunk = FileTransferChunk::transferEnd(m_transferId);
      m_events->addEvent(Event(EventTypes::FileTransferSending, m_eventTarget, chunk));
      m_complete = true;

      if (m_completeCallback) {
        m_completeCallback(true, "");
      }
      return false;
    }
    file = static_cast<std::ifstream *>(m_currentFile);
  }

  char buffer[kChunkSize];
  file->read(buffer, kChunkSize);
  size_t bytesRead = static_cast<size_t>(file->gcount());

  if (bytesRead > 0) {
    std::string data(buffer, bytesRead);
    auto *chunk = FileTransferChunk::fileData(m_transferId, data);
    m_events->addEvent(Event(EventTypes::FileTransferSending, m_eventTarget, chunk));

    m_currentFileBytesSent += bytesRead;
    m_bytesSent += bytesRead;

    if (m_progressCallback) {
      m_progressCallback(m_currentFileBytesSent, m_currentFileInfo.size, m_filesCompleted, m_totalFiles);
    }
  }

  if (file->eof() || m_currentFileBytesSent >= m_currentFileInfo.size) {
    auto *chunk = FileTransferChunk::fileEnd(m_transferId);
    m_events->addEvent(Event(EventTypes::FileTransferSending, m_eventTarget, chunk));

    m_filesCompleted++;
    file->close();

    LOG_DEBUG("file transfer: completed file '%s'", m_currentFileInfo.relativePath.c_str());

    if (!openNextFile()) {
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

  auto *file = static_cast<std::ifstream *>(m_currentFile);
  if (file && file->is_open()) {
    file->close();
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
  std::string infoStr = std::to_string(m_transferId) + ":" + std::to_string(m_totalFiles) + ":" + std::to_string(m_totalBytes);

  LOG_DEBUG("file transfer: drag info ready, %d files, %llu bytes", m_totalFiles, m_totalBytes);
}
