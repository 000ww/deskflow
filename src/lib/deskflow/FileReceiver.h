/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2024 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <string>

namespace deskflow {
class IStream;
}

/**
 * @brief File receiver state
 */
enum class FileReceiverState
{
  Waiting,      ///< Waiting for transfer to start
  ReceivingFile, ///< Receiving file data
  Complete,     ///< Transfer complete
  Error         ///< Transfer failed
};

/**
 * @brief Progress callback for receiver
 * @param bytesReceived Bytes received for current file
 * @param totalBytes Total bytes for current file (0 if unknown)
 * @param currentFile Current file being received
 */
using FileReceiveProgressCallback = std::function<void(
    uint64_t bytesReceived, uint64_t totalBytes, const std::string &currentFile
)>;

/**
 * @brief Completion callback for receiver
 * @param success True if all files received successfully
 * @param message Error message if failed
 */
using FileReceiveCompleteCallback = std::function<void(bool success, const std::string &message)>;

/**
 * @brief File receiver
 *
 * Handles receiving file data chunks and writing them to disk.
 */
class FileReceiver
{
public:
  /**
   * @brief Create a file receiver
   * @param transferId Transfer identifier
   * @param downloadDir Directory to save files to
   */
  FileReceiver(uint32_t transferId, const std::filesystem::path &downloadDir);
  ~FileReceiver();

  /**
   * @brief Process a file start chunk
   * @param relativePath Relative path of the file
   * @return True if successful
   */
  bool onFileStart(const std::string &relativePath);

  /**
   * @brief Process a file data chunk
   * @param data File content data
   * @return True if successful
   */
  bool onDataChunk(const std::string &data);

  /**
   * @brief Process a file end chunk
   * @return True if successful
   */
  bool onFileEnd();

  /**
   * @brief Process a transfer end chunk
   */
  void onTransferEnd();

  /**
   * @brief Process a transfer cancel chunk
   * @param reason Cancel reason
   */
  void onTransferCancel(const std::string &reason);

  /**
   * @brief Get current state
   */
  FileReceiverState state() const
  {
    return m_state;
  }

  /**
   * @brief Get transfer ID
   */
  uint32_t transferId() const
  {
    return m_transferId;
  }

  /**
   * @brief Set progress callback
   */
  void setProgressCallback(FileReceiveProgressCallback callback)
  {
    m_progressCallback = std::move(callback);
  }

  /**
   * @brief Set completion callback
   */
  void setCompleteCallback(FileReceiveCompleteCallback callback)
  {
    m_completeCallback = std::move(callback);
  }

  /**
   * @brief Get download directory
   */
  const std::filesystem::path &downloadDir() const
  {
    return m_downloadDir;
  }

private:
  /**
   * @brief Sanitize a relative path to prevent directory traversal
   * @param path Input path
   * @return Sanitized path
   */
  std::string sanitizePath(const std::string &path);

  /**
   * @brief Create parent directories if needed
   * @param path File path
   * @return True if successful
   */
  bool ensureParentDirectories(const std::filesystem::path &path);

  /// Transfer ID
  uint32_t m_transferId;

  /// Download directory
  std::filesystem::path m_downloadDir;

  /// Current state
  FileReceiverState m_state = FileReceiverState::Waiting;

  /// Current file being received
  std::ofstream m_currentFile;

  /// Current file path
  std::filesystem::path m_currentFilePath;

  /// Current temporary file path (for atomic writes)
  std::filesystem::path m_tempFilePath;

  /// Bytes received for current file
  uint64_t m_currentFileBytesReceived = 0;

  /// Total bytes received
  uint64_t m_totalBytesReceived = 0;

  /// Files received count
  uint32_t m_filesReceived = 0;

  /// Progress callback
  FileReceiveProgressCallback m_progressCallback;

  /// Completion callback
  FileReceiveCompleteCallback m_completeCallback;
};
