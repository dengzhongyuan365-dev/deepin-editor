// SPDX-FileCopyrightText: 2017 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "themelistmodel.h"
#include "../common/utils.h"
#include <QFileInfoList>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>
#include <QDir>

ThemeListModel::ThemeListModel(QObject *parent)
    : QAbstractListModel(parent)
{
    qDebug() << "ThemeListModel constructor";
    initThemes();
}

ThemeListModel::~ThemeListModel()
{
    qDebug() << "ThemeListModel destructor";
}

void ThemeListModel::setFrameColor(const QString &selectedColor, const QString &normalColor)
{
    qInfo() << "Setting frame colors - selected:" << selectedColor << "normal:" << normalColor;
    m_frameSelectedColor = selectedColor;
    m_frameNormalColor = normalColor;
}

void ThemeListModel::setSelection(const QString &path)
{
    qDebug() << "Setting selection for theme path:" << path;
    for (auto pair : m_themes) {
        if (path == pair.second) {
            const int row = m_themes.indexOf(pair);
            const QModelIndex &idx = QAbstractListModel::index(row, 0);
            emit requestCurrentIndex(idx);
            qDebug() << "Break, Selection set for theme path:" << path;
            break;
        }
    }
}

int ThemeListModel::rowCount(const QModelIndex &parent) const
{
    return m_themes.size();
}

QVariant ThemeListModel::data(const QModelIndex &index, int role) const
{
    qDebug() << "Getting data for row:" << index.row() << "role:" << role;
    const int r = index.row();

    const QString &name = m_themes.at(r).first;
    const QString &path = m_themes.at(r).second;

    switch (role) {
    case ThemeName:
        qDebug() << "Theme name:" << name;
        return name;
    case ThemePath:
        qDebug() << "Theme path:" << path;
        return path;
    case FrameNormalColor:
        qDebug() << "Frame normal color:" << m_frameNormalColor;
        return m_frameNormalColor;
    case FrameSelectedColor:
        qDebug() << "Frame selected color:" << m_frameSelectedColor;
        return m_frameSelectedColor;
    }

    qDebug() << "No data for role:" << role;
    return QVariant();
}

void ThemeListModel::initThemes()
{
    qDebug() << "Initializing theme list";
    QFileInfoList infoList = QDir(QString("%1share/deepin-editor/themes").arg(LINGLONG_PREFIX)).entryInfoList(QDir::Files | QDir::NoDotAndDotDot);

    for (QFileInfo info : infoList) {
        QVariantMap jsonMap = Utils::getThemeMapFromPath(info.filePath());
        QPair<QString, QString> pair;
        pair.first = jsonMap["metadata"].toMap()["name"].toString();
        pair.second = info.filePath();

        qDebug() << "Found theme:" << pair.first << "at path:" << pair.second;
        m_themes << pair;
    }

    qDebug() << "Sorting themes by background lightness";
    std::sort(m_themes.begin(), m_themes.end(),
              [=] (QPair<QString, QString> &a, QPair<QString, QString> &b) {
                  QVariantMap firstMap = Utils::getThemeMapFromPath(a.second);
                  QVariantMap secondMap = Utils::getThemeMapFromPath(b.second);

                  const QString &firstColor = firstMap["editor-colors"].toMap()["background-color"].toString();
                  const QString &secondColor = secondMap["editor-colors"].toMap()["background-color"].toString();

                  return QColor(firstColor).lightness() < QColor(secondColor).lightness();
              });

    qDebug() << "Themes:" << m_themes;
}
