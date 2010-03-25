/* This file is part of Clementine.

   Clementine is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Clementine is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Clementine.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "osd.h"

#include <QCoreApplication>
#include <QtDebug>
#include <QTextDocument>

using boost::scoped_ptr;

QDBusArgument& operator<< (QDBusArgument& arg, const QImage& image) {
  if (image.isNull()) {
    // Sometimes this gets called with a null QImage for no obvious reason.
    arg.beginStructure();
    arg << 0 << 0 << 0 << false << 0 << 0 << QByteArray();
    arg.endStructure();
    return arg;
  }
  QImage scaled = image.scaledToHeight(100, Qt::SmoothTransformation);
  QImage i = scaled.convertToFormat(QImage::Format_ARGB32).rgbSwapped();
  arg.beginStructure();
  arg << i.width();
  arg << i.height();
  arg << i.bytesPerLine();
  arg << i.hasAlphaChannel();
  int channels = i.isGrayscale() ? 1 : (i.hasAlphaChannel() ? 4 : 3);
  arg << i.depth() / channels;
  arg << channels;
  arg << QByteArray(reinterpret_cast<const char*>(i.bits()), i.numBytes());
  arg.endStructure();
  return arg;
}

const QDBusArgument& operator>> (const QDBusArgument& arg, QImage& image) {
  // This is needed to link but shouldn't be called.
  Q_ASSERT(0);
  return arg;
}

void OSD::Init() {
  interface_.reset(new org::freedesktop::Notifications(
      "org.freedesktop.Notifications",
      "/org/freedesktop/Notifications",
      QDBusConnection::sessionBus()));
  if (!interface_->isValid()) {
    qWarning() << "Error connecting to notifications service.";
  }
}

bool OSD::SupportsNativeNotifications() {
  return true;
}

bool OSD::SupportsTrayPopups() {
  return true;
}

void OSD::ShowMessageNative(const QString& summary, const QString& message,
                            const QString& icon) {
  QDBusPendingReply<uint> reply = interface_->Notify(
      QCoreApplication::applicationName(),
      0,
      icon,
      summary,
      message,
      QStringList(),
      QVariantMap(),
      timeout_);
  QDBusPendingCallWatcher* watcher = new QDBusPendingCallWatcher(reply, this);
  connect(watcher, SIGNAL(finished(QDBusPendingCallWatcher*)),
      SLOT(CallFinished(QDBusPendingCallWatcher*)));
}

void OSD::ShowMessageNative(const QString& summary, const QString& message,
                            const QImage& image) {
  QVariantMap hints;
  if (!image.isNull()) {
    hints["image_data"] = QVariant(image);
  }
  QDBusPendingReply<uint> reply = interface_->Notify(
      QCoreApplication::applicationName(),
      0,
      QString(),
      summary,
      message,
      QStringList(),
      hints,
      timeout_);
  QDBusPendingCallWatcher* watcher = new QDBusPendingCallWatcher(reply, this);
  connect(watcher, SIGNAL(finished(QDBusPendingCallWatcher*)),
      SLOT(CallFinished(QDBusPendingCallWatcher*)));
}

void OSD::CallFinished(QDBusPendingCallWatcher* watcher) {
  scoped_ptr<QDBusPendingCallWatcher> w(watcher);

  QDBusPendingReply<uint> reply = *watcher;
  if (reply.isError()) {
    qWarning() << "Error sending notification" << reply.error();
  }
}
