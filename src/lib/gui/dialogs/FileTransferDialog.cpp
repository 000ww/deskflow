/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2024 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "FileTransferDialog.h"
#include "ui_FileTransferDialog.h"

#include <QFileInfo>

FileTransferDialog::FileTransferDialog(
    uint32_t transferId, const std::string &sourceScreen, uint32_t fileCount, uint64_t totalSize, QWidget *parent
)
    : QDialog(parent),
      ui(new Ui::FileTransferDialog),
      m_transferId(transferId)
{
  ui->setupUi(this);

  // Set window flags
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

  // Set source screen name
  ui->labelSource->setText(QString::fromStdString(sourceScreen));

  // Set file count
  ui->labelFileCount->setText(tr("%n file(s)", "", static_cast<int>(fileCount)));

  // Set total size
  ui->labelTotalSize->setText(formatBytes(totalSize));

  // Connect buttons
  connect(ui->buttonAccept, &QPushButton::clicked, this, &FileTransferDialog::onAccept);
  connect(ui->buttonReject, &QPushButton::clicked, this, &FileTransferDialog::onReject);
}

FileTransferDialog::~FileTransferDialog()
{
  delete ui;
}

void FileTransferDialog::onAccept()
{
  m_accepted = true;
  accept();
}

void FileTransferDialog::onReject()
{
  m_accepted = false;
  reject();
}

QString FileTransferDialog::formatBytes(uint64_t bytes)
{
  const char *units[] = {"B", "KB", "MB", "GB", "TB"};
  int unitIndex = 0;
  double size = static_cast<double>(bytes);

  while (size >= 1024.0 && unitIndex < 4) {
    size /= 1024.0;
    unitIndex++;
  }

  if (unitIndex == 0) {
    return QString("%1 %2").arg(bytes).arg(units[unitIndex]);
  } else {
    return QString("%1 %2").arg(size, 0, 'f', 1).arg(units[unitIndex]);
  }
}
