/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2024 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "deskflow/FileReceiver.h"

#include "base/Log.h"

#include <filesystem>
#include <fstream>
#include <regex>

FileReceiver::FileReceiver(uint32_t transferId, const std::filesystem::path &downloadDir)
    : m_transferId(transferId),
      m_downloadDir(downloadDir)
{
}

FileReceiver::~FileReceiver()
{
  if (m_currentFile.is_open()) {
    m_currentFile.close();
    // Remove temp file if incomplete
    std::error_code ec;
    std::filesystem::remove(m_tempFilePath, ec);
  }
}

bool FileReceiver::onFileStart(const std::string &relativePath)
{
  // Close previous file if open
  if (m_currentFile.is_open()) {
    m_currentFile.close();
    // Remove temp file
    std::error_code ec;
    std::filesystem::remove(m_tempFilePath, ec);
  }

  // Sanitize path to prevent directory traversal
  std::string safePath = sanitizePath(relativePath);
  if (safePath.empty()) {
    LOG_ERR("file transfer: invalid file path: %s", relativePath.c_str());
    m_state = FileReceiverState::Error;
    if (m_completeCallback) {
      m_completeCallback(false, "Invalid file path: " + relativePath);
    }
    return false;
  }

  m_currentFilePath = m_downloadDir / safePath;

  // Check if path is a directory
  if (relativePath.back() == '/' || relativePath.back() == '\\') {
    std::error_code ec;
    std::filesystem::create_directories(m_currentFilePath, ec);
    if (ec) {
      LOG_ERR("file transfer: cannot create directory: %s: %s", m_currentFilePath.c_str(), ec.message().c_str());
      m_state = FileReceiverState::Error;
      if (m_completeCallback) {
        m_completeCallback(false, "Cannot create directory: " + ec.message());
      }
      return false;
    }
    LOG_DEBUG("file transfer: created directory '%s'", safePath.c_str());
    return true;
  }

  // Create parent directories
  if (!ensureParentDirectories(m_currentFilePath)) {
    m_state = FileReceiverState::Error;
    if (m_completeCallback) {
      m_completeCallback(false, "Cannot create parent directories");
    }
    return false;
  }

  // Open temp file for writing
  m_tempFilePath = m_currentFilePath.string() + ".deskflow.tmp";
  m_currentFile.open(m_tempFilePath, std::ios::binary | std::ios::trunc);
  if (!m_currentFile.is_open()) {
    LOG_ERR("file transfer: cannot create file: %s", m_tempFilePath.c_str());
    m_state = FileReceiverState::Error;
    if (m_completeCallback) {
      m_completeCallback(false, "Cannot create file: " + m_tempFilePath.string());
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
    // Ignore data chunks if not receiving a file
    // This can happen for directory entries
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

  // Report progress
  if (m_progressCallback) {
    m_progressCallback(m_currentFileBytesReceived, 0, m_currentFilePath.filename().string());
  }

  return true;
}

bool FileReceiver::onFileEnd()
{
  if (m_currentFile.is_open()) {
    m_currentFile.close();

    // Rename temp file to final name
    std::error_code ec;
    std::filesystem::rename(m_tempFilePath, m_currentFilePath, ec);
    if (ec) {
      LOG_ERR("file transfer: cannot rename temp file: %s", ec.message().c_str());
      m_state = FileReceiverState::Error;
      if (m_completeCallback) {
        m_completeCallback(false, "Cannot rename temp file: " + ec.message());
      }
      return false;
    }

    m_filesReceived++;
    LOG_DEBUG("file transfer: completed file '%s'", m_currentFilePath.filename().c_str());
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
    // Remove temp file
    std::error_code ec;
    std::filesystem::remove(m_tempFilePath, ec);
  }

  m_state = FileReceiverState::Error;
  LOG_WARN("file transfer: cancelled: %s", reason.c_str());

  if (m_completeCallback) {
    m_completeCallback(false, reason);
  }
}

std::string FileReceiver::sanitizePath(const std::string &path)
{
  // Remove leading/trailing whitespace
  std::string result = path;
  result.erase(0, result.find_first_not_of(" \t\n\r"));
  result.erase(result.find_last_not_of(" \t\n\r") + 1);

  // Replace backslashes with forward slashes
  std::replace(result.begin(), result.end(), '\\', '/');

  // Remove leading slashes
  while (!result.empty() && result[0] == '/') {
    result.erase(0, 1);
  }

  // Check for directory traversal
  if (result.find("..") != std::string::npos) {
    LOG_WARN("file transfer: path contains '..': %s", path.c_str());
    return "";
  }

  // Check for absolute paths
  if (!result.empty() && result[0] == '/') {
    LOG_WARN("file transfer: absolute path not allowed: %s", path.c_str());
    return "";
  }

  // Remove any null bytes
  result.erase(std::remove(result.begin(), result.end(), '\0'), result.end());

  return result;
}

bool FileReceiver::ensureParentDirectories(const std::filesystem::path &path)
{
  std::error_code ec;
  auto parent = path.parent_path();
  if (!parent.empty()) {
    std::filesystem::create_directories(parent, ec);
    if (ec) {
      LOG_ERR("file transfer: cannot create directory: %s: %s", parent.c_str(), ec.message().c_str());
      return false;
    }
  }
  return true;
}
