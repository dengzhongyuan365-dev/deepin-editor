// SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "config.h"

#include <iconv.h>

#include <QDebug>

#ifdef DTKCORE_CLASS_DConfigFile
#include <QStandardPaths>

const QString g_keyDisableImproveGB18030 = "disableImproveGB18030";
const QString g_keyDefaultEncoding = "defaultEncoding";
const QString g_keyEnablePatchedIconv = "enablePatchedIconv";
#endif

/**
   @brief 检测当前iconv使用的GB18030编码是否为2005标准，2005标准强制使用上层补丁版本
        通过检测2005和2022编码转的差异，以附录D中的编码为例验证
        2005标准 0xFE51 --> \u20087
        2022标准 0xFE51 --> \uE816
   @return iconv使用GB18030编码是否为2005标准，默认返回true
 */
bool detectIconvUse2005Standard()
{
    qDebug() << "Detecting iconv use GB18030-2005 standard";
    iconv_t handle = iconv_open("UTF-8", "GB18030");
    if (handle == reinterpret_cast<iconv_t>(-1)) {
        qWarning() << "Failed to open iconv handle for GB18030";
        return true;
    }

    QByteArray input("\xFE\x51");
    QByteArray output(input.size() * 2, 0);
    char *inputData = input.data();
    char *outputData = output.data();
    size_t inputLen = static_cast<size_t>(input.count());
    size_t outputLen = static_cast<size_t>(output.count());

    const size_t ret = iconv(handle, &inputData, &inputLen, &outputData, &outputLen);
    iconv_close(handle);

    if (ret == static_cast<size_t>(-1)) {
        qWarning() << "Failed to convert GB18030 to UTF-8";
        return true;
    }

    if (!output.contains("\uE816")) {
        qInfo() << "Current iconv uses GB18030-2005 standard";
        return true;
    }

    qInfo() << "Current iconv uses GB18030-2022 standard";
    return false;
}

/**
   @class Config
   @brief 获取DConfig配置
   @details 取得DConfig配置，目前主要用于配置是否提高GB18030识别率。
    配置文件路径为 /usr/share/dsg/configs/org.deepin.editor/org.deepin.editor.json
 */

Config::Config(QObject *parent)
    : QObject(parent)
    , encoding("UTF-8")
{
    qDebug() << "Initializing Config instance";
#ifdef DTKCORE_CLASS_DConfigFile
    dconfig = DConfig::create("org.deepin.editor", "org.deepin.editor");
    if (dconfig->isValid()) {
        qDebug() << "DConfig is valid";
        // 默认提高GB18030识别率
        improveGB18030 = !dconfig->value(g_keyDisableImproveGB18030).toBool();
        qInfo() << qPrintable("DConfig::ImproveGB18030:") << improveGB18030;

        patchedIconv = dconfig->value(g_keyEnablePatchedIconv).toBool();
        qInfo() << qPrintable("DConfig::enablePatchedIconv:") << patchedIconv;

        encoding = dconfig->value(g_keyDefaultEncoding).toByteArray().toUpper();
        qInfo() << qPrintable("DConfig::defaultEncoding") << encoding;

        connect(dconfig, &DConfig::valueChanged, this, [this](const QString &key) {
            qDebug() << "Config value changed for key:" << key;
            if (key == g_keyDisableImproveGB18030) {
                this->improveGB18030 = !this->dconfig->value(g_keyDisableImproveGB18030).toBool();
                qInfo() << qPrintable("DConfig::ImproveGB18030 changed:") << improveGB18030;
                qDebug() << "improveGB18030 changed to" << this->improveGB18030;
            } else if (key == g_keyEnablePatchedIconv) {
                this->patchedIconv = dconfig->value(g_keyEnablePatchedIconv).toBool();
                qInfo() << qPrintable("DConfig::enablePatchedIconv changed:") << patchedIconv;
                qDebug() << "patchedIconv changed to" << this->patchedIconv;
            } else if (key == g_keyDefaultEncoding) {
                this->encoding = dconfig->value(g_keyDefaultEncoding).toByteArray().toUpper();
                qInfo() << qPrintable("DConfig::defaultEncoding changed:") << encoding;
                qDebug() << "encoding changed to" << this->encoding;
            }
        });

    } else {
        qWarning() << qPrintable("org.deepin.editor DConfig not valid!");
    }
#else
    qWarning() << "DConfig is not supported by DTK!";
#endif

    // 检测当前iconv使用的GB18030编码是否为2005标准(2005标准强制使用上层补丁版本)
    iocnvUse2005Standard = detectIconvUse2005Standard();
    qDebug() << "Leaving Config::Config";
}

Config::~Config()
{
    qDebug() << "Destroying Config instance";
#ifdef DTKCORE_CLASS_DConfigFile
    if (dconfig) {
        qDebug() << "Deleting dconfig";
        delete dconfig;
    }
#endif
    qDebug() << "Leaving Config::~Config";
}

Config *Config::instance()
{
    qDebug() << "Getting Config singleton instance";
    static Config config;
    qDebug() << "Leaving Config::instance";
    return &config;
}

/**
   @return 返回是否提高GB18030编码识别率
 */
bool Config::enableImproveGB18030() const
{
    qDebug() << "Entering Config::enableImproveGB18030, returning" << improveGB18030;
    return improveGB18030;
}

/**
   @return 返回是否使用上层修改的适配2022标准的iconv设置，
    2005标准强制使用上层补丁版本，以适配2022要求
 */
bool Config::enablePatchedIconv() const
{
    qDebug() << "Entering Config::enablePatchedIconv, returning" << (patchedIconv || iocnvUse2005Standard);
    return patchedIconv || iocnvUse2005Standard;
}

/**
   @return 返回配置的默认编码，默认UTF-8
 */
QByteArray Config::defaultEncoding() const
{
    qDebug() << "Entering Config::defaultEncoding";
    if (encoding.isEmpty()) {
        qDebug() << "Encoding is empty, returning UTF-8";
        return QByteArray("UTF-8");
    }
    qDebug() << "Returning configured encoding:" << encoding;
    return encoding;
}
