/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2024 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QWidget>

#include <cstdint>
#include <string>

namespace Ui {
class TransferProgressWidget;
}

/**
 * @brief Transfer progress widget
 *
 * Shows progress for an active file transfer.
 */
class TransferProgressWidget : public QWidget
{
  Q_OBJECT

public:
  explicit TransferProgressWidget(QWidget *parent = nullptr);
  ~TransferProgressWidget() override;

  /**
   * @brief Update progress
   * @param bytesTransferred Bytes transferred for current file
   * @param totalBytes Total bytes for current file
   * @param filesCompleted Number of files completed
   * @param totalFiles Total number of files
   * @param currentFile Name of current file being transferred
   */
  void updateProgress(
      uint64_t bytesTransferred, uint64_t totalBytes, uint32_t filesCompleted, uint32_t totalFiles,
      const std::string &currentFile
  );

  /**
   * @brief Set transfer complete
   * @param success True if transfer was successful
   * @param message Status message
   */
  void setComplete(bool success, const std::string &message);

  /**
   * @brief Set transfer ID
   */
  void setTransferId(uint32_t transferId)
  {
    m_transferId = transferId;
  }

  /**
   * @brief Get transfer ID
   */
  uint32_t transferId() const
  {
    return m_transferId;
  }

signals:
  void cancelRequested(uint32_t transferId);

private slots:
  void onCancelClicked();

private:
  /**
   * @brief Format bytes to human readable string
   */
  static QString formatBytes(uint64_t bytes);

  /**
   * @brief Format speed to human readable string
   */
  static QString formatSpeed(uint64_t bytesPerSecond);

  Ui::TransferProgressWidget *ui;
  uint32_t m_transferId = 0;
  uint64_t m_lastBytesTransferred = 0;
  qint64 m_lastTime = 0;
};
