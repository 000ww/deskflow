/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2024 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QDialog>

#include <cstdint>
#include <string>

namespace Ui {
class FileTransferDialog;
}

/**
 * @brief File transfer request dialog
 *
 * Shows a dialog asking the user to accept or reject an incoming file transfer.
 */
class FileTransferDialog : public QDialog
{
  Q_OBJECT

public:
  /**
   * @brief Create a file transfer dialog
   * @param transferId Transfer identifier
   * @param sourceScreen Source screen name
   * @param fileCount Number of files
   * @param totalSize Total size in bytes
   * @param parent Parent widget
   */
  explicit FileTransferDialog(
      uint32_t transferId, const std::string &sourceScreen, uint32_t fileCount, uint64_t totalSize,
      QWidget *parent = nullptr
  );
  ~FileTransferDialog() override;

  /**
   * @brief Check if user accepted the transfer
   */
  bool isAccepted() const
  {
    return m_accepted;
  }

private slots:
  void onAccept();
  void onReject();

private:
  /**
   * @brief Format bytes to human readable string
   */
  static QString formatBytes(uint64_t bytes);

  Ui::FileTransferDialog *ui;
  uint32_t m_transferId;
  bool m_accepted = false;
};
