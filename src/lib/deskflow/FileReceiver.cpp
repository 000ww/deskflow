/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2024 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "deskflow/FileReceiver.h"

#include "base/Log.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <fstream>

FileReceiver::FileReceiver(uint32_t transferId, const std::string &downloadDir)
    : m_transferId(transferId),
      m_downloadDir(downloadDir)
{
}

FileReceiver::~FileReceiver()
{
  if (m_currentFile.is_open()) {
    m_currentFile.close();
    QFile::remove(QString::fromStdString(m_tempFilePath));
  }
}

bool FileReceiver::onFileStart(const std::string &relativePath)
{
  if (m_currentFile.is_open()) {
    m_currentFile.close();
    QFile::remove(QString::fromStdString(m_tempFilePath));
  }

  std::string safePath = sanitizePath(relativePath);
  if (safePath.empty()) {
    LOG_ERR("file transfer: invalid file path: %s", relativePath.c_str());
    m_state = FileReceiverState::Error;
    if (m_completeCallback) {
      m_completeCallback(false, "Invalid file path: " + relativePath);
    }
    return false;
  }

  m_currentFilePath = m_downloadDir + "/" + safePath;

  if (relativePath.back() == '/' || relativePath.back() == '\\') {
    QDir dir;
    if (!dir.mkpath(QString::fromStdString(m_currentFilePath))) {
      LOG_ERR("file transfer: cannot create directory: %s", m_currentFilePath.c_str());
      m_state = FileReceiverState::Error;
      if (m_completeCallback) {
        m_completeCallback(false, "Cannot create directory: " + m_currentFilePath);
      }
      return false;
    }
    LOG_DEBUG("file transfer: created directory '%s'", safePath.c_str());
    return true;
  }

  if (!ensureParentDirectories(m_currentFilePath)) {
    m_state = FileReceiverState::Error;
    if (m_completeCallback) {
      m_completeCallback(false, "Cannot create parent directories");
    }
    return false;
  }

  m_tempFilePath = m_currentFilePath + ".deskflow.tmp";
  m_currentFile.open(m_tempFilePath, std::ios::binary | std::ios::trunc);
  if (!m_currentFile.is_open()) {
    LOG_ERR("file transfer: cannot create file: %s", m_tempFilePath.c_str());
    m_state = FileReceiverState::Error;
    if (m_completeCallback) {
      m_completeCallback(false, "Cannot create file: " + m_tempFilePath);
    }
    return false;
  }

  m_currentFileBytesReceived = 0;
  m_state = FileReceiverState::ReceivingFile;

  LOG_DEBUG("file transfer: receiving file '%s'", safePath.c_str());
  return true;
}

bool FileReceiver::onDataChunk(const std::string &data)
{
  if (m_state != FileReceiverState::ReceivingFile || !m_currentFile.is_open()) {
    return true;
  }

  m_currentFile.write(data.data(), static_cast<std::streamsize>(data.size()));
  if (m_currentFile.fail()) {
    LOG_ERR("file transfer: write error");
    m_state = FileReceiverState::Error;
    if (m_completeCallback) {
      m_completeCallback(false, "Write error");
    }
    return false;
  }

  m_currentFileBytesReceived += data.size();
  m_totalBytesReceived += data.size();

  if (m_progressCallback) {
    std::string filename = m_currentFilePath.substr(m_currentFilePath.find_last_of("/\\") + 1);
    m_progressCallback(m_currentFileBytesReceived, 0, filename);
  }

  return true;
}

bool FileReceiver::onFileEnd()
{
  if (m_currentFile.is_open()) {
    m_currentFile.close();

    QFile::remove(QString::fromStdString(m_currentFilePath));
    if (!QFile::rename(QString::fromStdString(m_tempFilePath), QString::fromStdString(m_currentFilePath))) {
      LOG_ERR("file transfer: cannot rename temp file");
      m_state = FileReceiverState::Error;
      if (m_completeCallback) {
        m_completeCallback(false, "Cannot rename temp file");
      }
      return false;
    }

    m_filesReceived++;
    std::string filename = m_currentFilePath.substr(m_currentFilePath.find_last_of("/\\") + 1);
    LOG_DEBUG("file transfer: completed file '%s'", filename.c_str());
  }

  m_state = FileReceiverState::Waiting;
  return true;
}

void FileReceiver::onTransferEnd()
{
  if (m_currentFile.is_open()) {
    m_currentFile.close();
  }

  m_state = FileReceiverState::Complete;
  LOG_DEBUG("file transfer: all files received, %d files total", m_filesReceived);

  if (m_completeCallback) {
    m_completeCallback(true, "");
  }
}

void FileReceiver::onTransferCancel(const std::string &reason)
{
  if (m_currentFile.is_open()) {
    m_currentFile.close();
    QFile::remove(QString::fromStdString(m_tempFilePath));
  }

  m_state = FileReceiverState::Error;
  LOG_WARN("file transfer: cancelled: %s", reason.c_str());

  if (m_completeCallback) {
    m_completeCallback(false, reason);
  }
}

std::string FileReceiver::sanitizePath(const std::string &path)
{
  std::string result = path;
  result.erase(0, result.find_first_not_of(" \t\n\r"));
  result.erase(result.find_last_not_of(" \t\n\r") + 1);

  std::replace(result.begin(), result.end(), '\\', '/');

  while (!result.empty() && result[0] == '/') {
    result.erase(0, 1);
  }

  if (result.find("..") != std::string::npos) {
    LOG_WARN("file transfer: path contains '..': %s", path.c_str());
    return "";
  }

  if (!result.empty() && result[0] == '/') {
    LOG_WARN("file transfer: absolute path not allowed: %s", path.c_str());
    return "";
  }

  result.erase(std::remove(result.begin(), result.end(), '\0'), result.end());

  return result;
}

bool FileReceiver::ensureParentDirectories(const std::string &path)
{
  QFileInfo fi(QString::fromStdString(path));
  QDir dir = fi.absoluteDir();
  if (!dir.exists()) {
    if (!dir.mkpath(".")) {
      LOG_ERR("file transfer: cannot create directory: %s", dir.absolutePath().toStdString().c_str());
      return false;
    }
  }
  return true;
}
