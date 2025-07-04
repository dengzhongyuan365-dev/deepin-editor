// SPDX-FileCopyrightText: 2019 - 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "detectcode.h"
#include "../common/config.h"

#include <QByteArray>
#include <QString>
#include <QDebug>
#include <QDateTime>
#include <QTextCodec>
#include <QRegularExpression>

#include <stdio.h>

#define DISABLE_TEXTCODEC

QMap<QString, QByteArray> DetectCode::sm_LangsMap;

// 最低的检测准确度判断，低于90%需要调整策略
static const float gs_dMinConfidence = 0.9f;

// 手动添加 UTF BOM 信息
static QMap<QString, QByteArray> gs_byteOrderMark = {{"UTF-16LE", QByteArray::fromHex("FFFE")},
                                                     {"UTF-16BE", QByteArray::fromHex("FEFF")},
                                                     {"UTF-32LE", QByteArray::fromHex("FFFE0000")},
                                                     {"UTF-32BE", QByteArray::fromHex("0000FEFF")}};

/**
  FIXME: 临时方案，后续修改iconv等编码转换处理
  # GB18030-2022 - UTF-8 编码转换处理
  为适配 GB18030-2022 规范，{gs_UTF8MapGB18030Data} 为 GB18030-UTF8 PUA 区域映射表，对应 GB18030-2022 规范附录D 表D.1
  替换2005和2022规范中转换存在差异的部分， 见 \l{https://en.wikipedia.org/wiki/GB_18030}
 */
static QHash<QByteArray, quint32> gs_UTF8MapGB18030Data{
    {"\uE81E", 0x37903582}, {"\uE826", 0x38903582}, {"\uE82B", 0x39903582}, {"\uE82C", 0x30913582}, {"\uE832", 0x31913582},
    {"\uE843", 0x32913582}, {"\uE854", 0x33913582}, {"\uE864", 0x34913582}, {"\uE78D", 0x36823184}, {"\uE78F", 0x37823184},
    {"\uE78E", 0x38823184}, {"\uE790", 0x39823184}, {"\uE791", 0x30833184}, {"\uE792", 0x31833184}, {"\uE793", 0x32833184},
    {"\uE794", 0x33833184}, {"\uE795", 0x34833184}, {"\uE796", 0x35833184}, /*{"\uE816", 0x31903295}, {"\uE817", 0x33903295},
    {"\uE818", 0x30973295}, {"\uE831", 0x37B93695}, {"\uE83B", 0x35BA3096}, {"\uE855", 0x30B63596}*/};

/**
  # GB18030-2022 规范附录D 表D.2处理
  GB18030 -> UTF8 编码转换时需要特殊处理，保留GB18030特殊编码的字符
  在转换 0xFE51...0xFE91 编码时，GB18030 规范和 Unciode 4.1 规范转换后的编码不同，以 0xFE51 为例:
  1. 按 GB18030-2022 编码规范，0xFE51 转换为 \uE816 ，而非 \u20087，因此在转码前将 0xFE51 替换为转码标识 0xFFFFFF01 ;
  2. iconv 在转码遇到 0xFFFFFF01 时将报错退出，此时将 0xFFFFFF01 转换为 GB18030-2022 编码 \uE816 ;
  3. 由于根据 Unciode 4.1 规范，转换 UTF-8 到 GB18030 编码时，\uE816 不应存在，此时同样保存，手动还原编码为 0xFE51。
 */
static QHash<QByteArray, QByteArray> gs_ReplaceFromGB18030_2005Error{
    {QByteArray::fromHex("FE51"), QByteArray::fromHex("FFFFFF01")},
    {QByteArray::fromHex("FE52"), QByteArray::fromHex("FFFFFF02")},
    {QByteArray::fromHex("FE53"), QByteArray::fromHex("FFFFFF03")},
    {QByteArray::fromHex("FE6C"), QByteArray::fromHex("FFFFFF04")},
    {QByteArray::fromHex("FE76"), QByteArray::fromHex("FFFFFF05")},
    {QByteArray::fromHex("FE91"), QByteArray::fromHex("FFFFFF06")}};
static QHash<QByteArray, QByteArray> gs_ReplaceToUTF8_2005Error{{"\uE816", QByteArray::fromHex("FFFFFF01")},
                                                                {"\uE817", QByteArray::fromHex("FFFFFF02")},
                                                                {"\uE818", QByteArray::fromHex("FFFFFF03")},
                                                                {"\uE831", QByteArray::fromHex("FFFFFF04")},
                                                                {"\uE83B", QByteArray::fromHex("FFFFFF05")},
                                                                {"\uE855", QByteArray::fromHex("FFFFFF06")}};
static QHash<QByteArray, QByteArray> gs_ReplaceUtf8ToGB18030_2005Error{{"\uE816", QByteArray::fromHex("FE51")},
                                                                       {"\uE817", QByteArray::fromHex("FE52")},
                                                                       {"\uE818", QByteArray::fromHex("FE53")},
                                                                       {"\uE831", QByteArray::fromHex("FE6C")},
                                                                       {"\uE83B", QByteArray::fromHex("FE76")},
                                                                       {"\uE855", QByteArray::fromHex("FE91")}};

/**
  同样，由于 0xFE51 和 0x95329031 在 Unciode 4.1 规范，均转换为 \u20087 ，但反向转换时，\u20087 转换为 0xFE51 。
  导致数据互转时，源数据出现了变更，因此同样需要特殊处理，在 UTF-8 转换为 GB18030-2022 时，将 0xF0A08287(\u20087) 替换,
  并在实际转换过程中替换为 0x95329031 ，保证 GB18030-2022 编码转换准确
 */
static QHash<QByteArray, QByteArray> gs_ReplaceFromUtf8_2020Error{
    {QByteArray::fromHex("95329031"), QByteArray::fromHex("FFFF11")},
    {QByteArray::fromHex("95329033"), QByteArray::fromHex("FFFF12")},
    {QByteArray::fromHex("95329730"), QByteArray::fromHex("FFFF13")},
    {QByteArray::fromHex("9536B937"), QByteArray::fromHex("FFFF14")},
    {QByteArray::fromHex("9630BA35"), QByteArray::fromHex("FFFF15")},
    {QByteArray::fromHex("9635B630"), QByteArray::fromHex("FFFF16")},
};
// 0xF0A08287 为 \u20087 的 UTF-8 HEX 编码
static QHash<QByteArray, QByteArray> gs_ReplaceToGB18030_2020Error{
    {QByteArray::fromHex("F0A08287"), QByteArray::fromHex("FFFF11")},
    {QByteArray::fromHex("F0A08289"), QByteArray::fromHex("FFFF12")},
    {QByteArray::fromHex("F0A0838C"), QByteArray::fromHex("FFFF13")},
    {QByteArray::fromHex("F0A19797"), QByteArray::fromHex("FFFF14")},
    {QByteArray::fromHex("F0A2A68F"), QByteArray::fromHex("FFFF15")},
    {QByteArray::fromHex("F0A487BE"), QByteArray::fromHex("FFFF16")},
};
static QHash<QByteArray, QByteArray> gs_ReplaceFromUtf8ToGB18030_2020Error{
    {QByteArray::fromHex("95329031"), "\u20087"},
    {QByteArray::fromHex("95329033"), "\u20089"},
    {QByteArray::fromHex("95329730"), "\u200CC"},
    {QByteArray::fromHex("9536B937"), "\u215D7"},
    {QByteArray::fromHex("9630BA35"), "\u2298F"},
    {QByteArray::fromHex("9635B630"), "\u241FE"},
};

// 见QTextCodec::mibEnum()
enum MibEncoding {
    UnknownMib = 0,
    UTF_8 = 106,
    GB18030 = 114,
    UTF_16BE = 1013,
    UTF_16LE = 1014,
    UTF_16 = 1015,
    UTF_32 = 1017,
    UTF_32BE = 1018,
    UTF_32LE = 1019,
};

DetectCode::DetectCode() {}

/**
 * @brief 根据文件头内容 \a content 取得文件 \a filepath 字符编码格式
 * @param filepath  待获取字符编码文件
 * @param content   文件头内容
 * @return 文件字符编码格式
 *
 * @note 对于大文本文件，文件头内容 \a content 可能在文件中间截断，\a content 尾部带有被截断的字符，
 *      极大的降低字符编码识别率。为此，在识别率过低时裁剪尾部数据，重新检测以提高文本识别率。
 */
QByteArray DetectCode::GetFileEncodingFormat(QString filepath, QByteArray content)
{
    qDebug() << "Enter GetFileEncodingFormat";
    QString charDetectedResult;
    QByteArray ucharDetectdRet;
    QByteArrayList icuDetectRetList;
    QByteArray detectRet;
    float chardetconfidence = 0.0f;

    /* chardet识别编码 */
    QString str(content);
    // 匹配的是中文(仅在UTF-8编码下)
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
    QRegExp reg("[\\x4e00-\\x9fa5]+");
#else
    QRegularExpression reg("[\\x4e00-\\x9fa5]+");
#endif
    bool bFlag = str.contains(reg);
    if (bFlag) {
        qDebug() << "Content contains Chinese characters";
        const QByteArray suffix = "为增加探测率保留的中文";
        QByteArray newContent = content;
        //手动添加中文字符，避免字符长度太短而导致判断编码错误
        newContent += suffix;
        DetectCode::ChartDet_DetectingTextCoding(newContent, charDetectedResult, chardetconfidence);
        qDebug() << "Initial chardet confidence for Chinese content:" << chardetconfidence;

        // 大文本数据存在从文档中间截断的可能，导致unicode中文字符被截断，解析为乱码，处理部分情况
        // 根据文本中断的情况，每次尝试解析编码后移除尾部字符，直到识别率达到90%以上
        int tryCount = 5;
        while (chardetconfidence < gs_dMinConfidence && newContent.size() > suffix.size() && tryCount-- > 0) {
            qDebug() << "Confidence too low, retrying. Try count left:" << tryCount << ", current confidence:" << chardetconfidence;
            // 移除可能的乱码尾部字符
            newContent.remove(newContent.size() - suffix.size(), 1);
            DetectCode::ChartDet_DetectingTextCoding(newContent, charDetectedResult, chardetconfidence);
        }
        qDebug() << "Final chardet confidence after retries:" << chardetconfidence;
    } else {
        qDebug() << "Content does not contain Chinese characters";
        DetectCode::ChartDet_DetectingTextCoding(content, charDetectedResult, chardetconfidence);

        // 部分非unicode编码同为中文，例如 GB18030, BIG5 等中文编码，同样判断识别率，识别率较低手动干预多次检测
        int tryCount = 5;
        QByteArray newContent = content;
        while (chardetconfidence < gs_dMinConfidence && !newContent.isEmpty() && tryCount-- > 0) {
            newContent.chop(1);
            DetectCode::ChartDet_DetectingTextCoding(newContent, charDetectedResult, chardetconfidence);
        }
        qDebug() << "Final chardet confidence for non-Chinese after retries:" << chardetconfidence;
    }
    ucharDetectdRet = charDetectedResult.toLatin1();

    // uchardet识别编码 若识别率过低, 考虑是否非单字节编码格式。
    if (ucharDetectdRet.contains("unknown") || ucharDetectdRet.contains("ASCII") || ucharDetectdRet.contains("???") ||
        ucharDetectdRet.isEmpty() || chardetconfidence < gs_dMinConfidence) {
        qDebug() << "Uchardet detection needed or confidence too low, calling UchardetCode";
        ucharDetectdRet = DetectCode::UchardetCode(filepath);
    }

    if (ucharDetectdRet.contains("ASCII")) {
        qDebug() << "Detected ASCII, using default encoding";
        // 使用配置的默认文件编码，默认为UTF-8
        detectRet = Config::instance()->defaultEncoding();
    } else {
        qDebug() << "Detected non-ASCII, trying ICU and selecting coding";
        // icu识别编码
        icuDetectTextEncoding(filepath, icuDetectRetList);
        detectRet = selectCoding(ucharDetectdRet, icuDetectRetList, chardetconfidence);

        if (detectRet.contains("ASCII") || detectRet.isEmpty()) {
            qDebug() << "Selected encoding is ASCII or empty, using default encoding";
            // 使用配置的默认文件编码，默认为UTF-8
            detectRet = Config::instance()->defaultEncoding();
        }
    }

    qDebug() << "GetFileEncodingFormat exit, detected encoding:" << detectRet.toUpper();
    qDebug() << "Exit GetFileEncodingFormat";
    return detectRet.toUpper();
}

QByteArray DetectCode::UchardetCode(QString filepath)
{
    qDebug() << "Enter UchardetCode, file:" << filepath;

    FILE *fp;
    QByteArray charset;

    size_t buffer_size = 0x10000;
    char *buff = new char[buffer_size];
    memset(buff, 0, buffer_size);

    /* 通过样本字符分析文本编码 */
    uchardet_t handle = uchardet_new();

    /* 打开被检测文本文件，并读取一定数量的样本字符 */
    fp = fopen(filepath.toLocal8Bit().data(), "rb");

    if (fp) {
        qDebug() << "File opened successfully for UchardetCode:" << filepath;
        while (!feof(fp)) {
            size_t len = fread(buff, 1, buffer_size, fp);
            qDebug() << "Reading data for UchardetCode, len:" << len;
            int retval = uchardet_handle_data(handle, buff, len);
            if (retval != 0) {
                qWarning() << "uchardet_handle_data returned non-zero value:" << retval;
                continue;
            }

            break;
        }
        fclose(fp);

        uchardet_data_end(handle);
        charset = uchardet_get_charset(handle);
        qDebug() << "Uchardet detected charset:" << charset;
    } else {
        qWarning() << "Failed to open file for UchardetCode:" << filepath;
    }

    uchardet_delete(handle);
    delete[] buff;
    buff = nullptr;

    if (charset == "MAC-CENTRALEUROPE") {
        qDebug() << "Correcting charset from MAC-CENTRALEUROPE";
        charset = "MACCENTRALEUROPE";
    }
    if (charset == "MAC-CYRILLIC") {
        qDebug() << "Correcting charset from MAC-CYRILLIC";
        charset = "MACCYRILLIC";
    }
    if (charset.contains("WINDOWS-")) {
        qDebug() << "Correcting charset from WINDOWS-";
        charset = charset.replace("WINDOWS-", "CP");
    }

    qDebug() << "Exit UchardetCode";
    return charset;
}

/**
 * @author guoshao
 * @brief  icuDetectTextEncoding() icu库编码识别
 * @param  filePath:文件路径，listDetectRet:编码识别结果存在的变量
 **/
void DetectCode::icuDetectTextEncoding(const QString &filePath, QByteArrayList &listDetectRet)
{
    qDebug() << "Enter icuDetectTextEncoding, file:" << filePath;
    FILE *file;
    file = fopen(filePath.toLocal8Bit().data(), "rb");
    if (file == nullptr) {
        qWarning() << "Failed to open file for icuDetectTextEncoding:" << filePath;
        qInfo() << "fopen file failed.";
        return;
    }

    size_t iBuffSize = 4096;
    char *detected = nullptr;
    char *buffer = new char[iBuffSize];
    memset(buffer, 0, iBuffSize);

    int readed = 0;
    while (!feof(file)) {
        size_t len = fread(buffer, 1, iBuffSize, file);
        readed += len;
        qDebug() << "Reading data for icuDetectTextEncoding, len:" << len << ", total read:" << readed;
        if (readed > 1 * 1024 * 1024) {
            qDebug() << "Read limit exceeded (1MB), breaking loop";
            break;
        }

        if (detectTextEncoding(buffer, len, &detected, listDetectRet)) {
            qDebug() << "Text encoding detected, breaking loop";
            break;
        }
    }

    delete[] buffer;
    buffer = nullptr;
    fclose(file);
    qDebug() << "Exit icuDetectTextEncoding, detected encodings:" << listDetectRet;
}

/**
 * @author guoshao
 * @brief  detectTextEncoding() icu库编码识别内层函数
 * @param  data:要识别的内容，len:要识别的内容的长度，detected:编码识别结果存在的变量，
 *         listDetectRet:编码识别结果存在的list
 * @return true:识别成功，false:识别失败
 **/
bool DetectCode::detectTextEncoding(const char *data, size_t len, char **detected, QByteArrayList &listDetectRet)
{
    qDebug() << "[DetectCode] Enter detectTextEncoding, data size:" << len;

    Q_UNUSED(detected);

    UCharsetDetector *csd;
    const UCharsetMatch **csm;
    int32_t matchCount = 0;

    UErrorCode status = U_ZERO_ERROR;
    csd = ucsdet_open(&status);
    if (status != U_ZERO_ERROR) {
        qWarning() << "ucsdet_open failed, status:" << status;
        return false;
    }

    ucsdet_setText(csd, data, len, &status);
    if (status != U_ZERO_ERROR) {
        qWarning() << "ucsdet_setText failed, status:" << status;
        return false;
    }

    csm = ucsdet_detectAll(csd, &matchCount, &status);
    if (status != U_ZERO_ERROR) {
        qWarning() << "ucsdet_detectAll failed, status:" << status;
        return false;
    }
    qDebug() << "matchCount from ucsdet_detectAll:" << matchCount;

    int readMax = 0;
    // 提高GB18030识别率时，拓展允许读取的编码列表
    if (Config::instance()->enableImproveGB18030()) {
        qDebug() << "Improving GB18030 recognition, setting readMax to min(6, matchCount)";
        readMax = qMin(6, matchCount);
    } else {
        qDebug() << "Not improving GB18030 recognition, setting readMax to min(3, matchCount)";
        readMax = qMin(3, matchCount);
    }
    qDebug() << "Final readMax:" << readMax;

    for (int i = 0; i < readMax; i++) {
        auto str = ucsdet_getName(csm[i], &status);
        if (status != U_ZERO_ERROR) {
            qWarning() << "ucsdet_getName failed for index" << i << ", status:" << status;
            return false;
        }
        listDetectRet << QByteArray(str);
        qDebug() << "Detected encoding:" << str << "(index:" << i << ")";
    }

    ucsdet_close(csd);
    qDebug() << "Exit detectTextEncoding, result:" << !listDetectRet.isEmpty();

    return true;
}

/**
 * @author guoshao
 * @brief  selectCoding() 筛选识别出来的编码
 * @param  ucharDetectdRet:chardet/uchardet识别的编码结果，icuDetectRetList:编码识别结果存在的list
 * @return 筛选编码的结果
 **/
QByteArray DetectCode::selectCoding(QByteArray ucharDetectdRet, QByteArrayList icuDetectRetList, float confidence)
{
    qDebug() << "[DetectCode] Enter selectCoding, uchardet:" << ucharDetectdRet
             << "icu:" << icuDetectRetList << "confidence:" << confidence;

    // 列表不允许为空
    if (icuDetectRetList.isEmpty()) {
        qDebug() << "[DetectCode] Exit selectCoding, selected:" << (icuDetectRetList.isEmpty() ? QByteArray() : icuDetectRetList[0]);
        return QByteArray();
    }

    if (!ucharDetectdRet.isEmpty()) {
        qDebug() << "ucharDetectdRet is not empty";
        // 获取是否允许提高GB18030编码的策略
        if (Config::instance()->enableImproveGB18030()) {
            qDebug() << "GB18030 improvement enabled";
            // 中文环境优先匹配GB18030编码
            if (QLocale::Chinese == QLocale::system().language()) {
                qDebug() << "System language is Chinese";
                if (confidence < gs_dMinConfidence && icuDetectRetList.contains("GB18030")) {
                    qDebug() << "Confidence low and GB18030 in list, returning GB18030";
                    return QByteArray("GB18030");
                }
            }
        }

        if (ucharDetectdRet.contains(icuDetectRetList[0])) {
            qDebug() << "ucharDetectdRet contains first ICU result, returning uchardetDetectRet";
            return ucharDetectdRet;
        } else {
            qDebug() << "ucharDetectdRet does not contain first ICU result";
            if (icuDetectRetList.contains("GB18030")) {
                qDebug() << "ICU list contains GB18030, returning GB18030";
                return QByteArray("GB18030");
            } else {
                // 筛选部分带后缀的编码格式，例如 UTF-16 BE 和 UTF-16
                if (icuDetectRetList[0].contains(ucharDetectdRet)) {
                    qDebug() << "First ICU result contains uchardetDetectRet, returning first ICU result";
                    return icuDetectRetList[0];
                }
                qDebug() << "Returning uchardetDetectRet as fallback";
                return ucharDetectdRet;
            }
        }
    }

    if (ucharDetectdRet.isEmpty()) {
        qDebug() << "ucharDetectdRet is empty, checking ICU list for GB18030";
        if (icuDetectRetList.contains("GB18030")) {
            qDebug() << "ICU list contains GB18030, returning GB18030";
            return QByteArray("GB18030");
        } else {
            qDebug() << "ICU list does not contain GB18030, returning first ICU result";
            return icuDetectRetList[0];
        }
    }

    qDebug() << "Exit selectCoding, selected:" << (icuDetectRetList.isEmpty() ? QByteArray() : icuDetectRetList[0]);
    return QByteArray();
}

#if 0 /* 因为开源协议存在法律冲突，停止使用libenca0编码识别库 */
QByteArray DetectCode::EncaDetectCode(QString filepath)
{
    /*
     * 编码区域对应的简称
     *"zh"中文 "be"俄罗斯 "bg"保加利亚 "cs"捷克文 "et"爱沙尼亚语 "hr"克罗地亚人[语]; "hu"匈牙利语 "lt"立陶宛
     * "lv"拉脱维亚语 "pl"波兰语 "ru"俄语 "sk"斯洛伐克人（语）  "sl"斯洛文尼亚人（语）  "uk"乌克兰人（语）
     */

    sm_LangsMap.clear();
    const char *langArray[] = {"zh", "be", "bg", "cs", "et", "hr", "hu", "lt", "lv", "pl", "ru", "sk", "sl", "uk"};
#if 0 /* EncaAnalyserState */
    size_t size;
    const char **lang = enca_get_languages(&size);
    QStringList langs;
    for (size_t i = 0; i < size; i++) {
        langs << lang[i];
    }
#endif

    QByteArray charset;

    for (size_t i = 0; i < sizeof(langArray) / sizeof(const char *); i++) {

        EncaAnalyser analyser = nullptr;
        analyser = enca_analyser_alloc(langArray[i]);
        enca_set_threshold(analyser, 1.38);
        enca_set_multibyte(analyser, 1);
        enca_set_ambiguity(analyser, 1);
        enca_set_garbage_test(analyser, 1);

        size_t buffer_size = 0x10000;

        unsigned char *buff = new unsigned char[buffer_size];
        memset(buff, 0, buffer_size);

        /* 打开被检测文本文件，并读取一定数量的样本字符 */
        FILE *fp; /* the processed file */
        fp = fopen(filepath.toLocal8Bit().data(), "rb");

        if (fp) {
            while (!feof(fp)) {
                size_t len = fread(buff, 1, buffer_size, fp);
                EncaEncoding encoding = enca_analyse(analyser, buff, len);
                charset = enca_charset_name(encoding.charset, EncaNameStyle::ENCA_NAME_STYLE_MIME);
                qDebug() << QStringLiteral("ENCA文本的编码方式是:") << charset;
                //识别文本编码识别
                if (encoding.charset == -1) continue;
                break;
            }

            enca_analyser_free(analyser);
            analyser = nullptr;

            delete [] buff;
            buff = nullptr;

            fclose(fp);
        } else {
            qDebug() << QStringLiteral("ENCA打开失败:") << filepath;
        }

        if (charset == "US-ASCII") charset = "ASCII";
        if (charset == "GB2312" || charset == "GBK") charset = "GB18030";
        if (charset == "ISO-10646-UCS-2") charset = "UTF-16BE";
        sm_LangsMap[langArray[i]] = charset;

        if (!charset.isEmpty()) {
            break;
        }
    }

    return charset;
}
#endif

/**
 * @brief ChartDet_DetectingTextCoding libchardet1编码识别库识别编码
 */
int DetectCode::ChartDet_DetectingTextCoding(const char *str, QString &encoding, float &confidence)
{
    DetectObj *obj = detect_obj_init();

    if (obj == nullptr) {
        // qInfo() << "Memory Allocation failed\n";
        return CHARDET_MEM_ALLOCATED_FAIL;
    }

    /* 另一种编码识别逻辑，暂且保留*/
    /*size_t buffer_size = 1024;
    char *buff = new char[buffer_size];
    memset(buff, 0, buffer_size);

    FILE *fp;
    fp = fopen(filepath.toLocal8Bit().data(), "rb");
    if (fp) {
        while (!feof(fp)) {
            size_t len = fread(buff, 1, buffer_size, fp);
            detect(buff, &obj);
            QString encoding = obj->encoding;
            qInfo() << "==== encoding: " << encoding;
            if (encoding == "") {
                continue;
            } else if (!encoding.isEmpty()) {
                break;
            }
            fclose(fp);
        }
    }

    delete [] buff;
    buff = nullptr;*/

#ifndef CHARDET_BINARY_SAFE
    // before 1.0.5. This API is deprecated on 1.0.5
    switch (detect(str, &obj))
#else
    // from 1.0.5
    switch (detect_r(str, strlen(str), &obj))
#endif
    {
        case CHARDET_OUT_OF_MEMORY:
            qInfo() << "On handle processing, occured out of memory\n";
            detect_obj_free(&obj);
            return CHARDET_OUT_OF_MEMORY;
        case CHARDET_NULL_OBJECT:
            qInfo() << "2st argument of chardet() is must memory allocation with detect_obj_init API\n";
            return CHARDET_NULL_OBJECT;
    }

#ifndef CHARDET_BOM_CHECK
        // qInfo() << "encoding:" << obj->encoding << "; confidence: " << obj->confidence;
#else
    // from 1.0.6 support return whether exists BOM
    qInfo() << "encoding:" << obj->encoding << "; confidence: " << obj->confidence << "; bom: " << obj->bom;
#endif

    encoding = obj->encoding;
    confidence = obj->confidence;
    detect_obj_free(&obj);

    return CHARDET_SUCCESS;
}

/**
 * @return 根据 UTF-8 字符编码，返回传入的字符串 \a buf 头部字符可能占用的字节数量
 * @note 不同编码UTF-8字节划分示例，除首个字节为0外
 *  如果一个字节的第一位是0，则这个字节单独就是一个字符；如果第一位是1，则连续有多少个1，就表示当前字符占用多少个字节。
 *  0000 0000-0000 007F | 0xxxxxxx
 *  0000 0080-0000 07FF | 110xxxxx 10xxxxxx
 *  0000 0800-0000 FFFF | 1110xxxx 10xxxxxx 10xxxxxx
 *  0001 0000-0010 FFFF | 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
 *
 * @link https://zh.wikipedia.org/wiki/UTF-8
 */
int utf8MultiByteCount(char *buf, size_t size)
{
    qDebug() << "utf8MultiByteCount() entry, size:" << size;
    // UTF 字符类型，单字符，中间字符，双字节，三字节，四字节
    enum UtfCharType {
        Single,
        Mid,
        DoubleBytes,
        ThreeBytes,
        FourBytes,
    };

    // 取得 UTF-8 字节序数值
    auto LeftBitFunc = [](char data) -> int {
        // 返回前导1的个数
        int res = 0;
        while (data & 0x80) {
            res++;
            data <<= 1;
        }
        return res;
    };

    int count = 0;
    while (size > 0 && count < FourBytes) {
        qDebug() << "Looping in utf8MultiByteCount, current size:" << size << ", count:" << count;
        int leftBits = LeftBitFunc(*buf);
        qDebug() << "Left bits for current byte:" << leftBits;
        switch (leftBits) {
            case Mid:
                qDebug() << "Case Mid, incrementing count";
                count++;
                break;
            case DoubleBytes:
                qDebug() << "Case DoubleBytes, returning leftBits";
                return leftBits;
            case ThreeBytes:
                qDebug() << "Case ThreeBytes, returning leftBits";
                return leftBits;
            case FourBytes:
                qDebug() << "Case FourBytes, returning leftBits";
                return leftBits;
            default:
                qDebug() << "Case default (single byte or >4 bytes), returning 1";
                // 超过4字节或单字节，均返回1
                return 1;
        }

        buf++;
        size--;
    }

    qDebug() << "utf8MultiByteCount() exit, returning count:" << count;
    return count;
}

/**
   @brief 检测 GB18030 转换 UTF-8 时PUA区域异常处理

   @note GB18030 PUA区域编码转换各版本异同
   iconv当前使用2022标志，遇到PUA区域将报错(2022不再支持)，此处兼容2005设计，
   GB18030 - UTF-8 互转时，将PUA区域字符按2005标准转换
   | GB18030 原始数据（HEX）  | GB18030-2005 to UTF8  | GB18030-2022 toUtf8   |
   |------------------------|-----------------------|-----------------------|
   | 0x37903582             |		\uE81E          |		\u9FB4          |
   | 0x38903582             |		\uE826          |		\u9FB5          |
   | 0x39903582             |		\uE82B          |		\u9FB6          |
   | 0x30913582             |		\uE82C          |		\u9FB7          |
   | 0x31913582             |		\uE832          |		\u9FB8          |
   | 0x32913582             |		\uE843          |		\u9FB9          |
   | 0x33913582             |		\uE854          |		\u9FBA          |
   | 0x34913582             |		\uE864          |		\u9FBB          |
   | 0x36823184             |		\uE78D          |		\uFE10          |
   | 0x37823184             |		\uE78F          |		\uFE11          |
   | 0x38823184             |		\uE78E          |		\uFE12          |
   | 0x39823184             |		\uE790          |		\uFE13          |
   | 0x30833184             |		\uE791          |		\uFE14          |
   | 0x31833184             |		\uE792          |		\uFE15          |
   | 0x32833184             |		\uE793          |		\uFE16          |
   | 0x33833184             |		\uE794          |		\uFE17          |
   | 0x34833184             |		\uE795          |		\uFE18          |
   | 0x35833184             |		\uE796          |		\uFE19          |

   以下为特殊字符，2005和2022转换后编码，映射Unicode编码
   | GB18030 原始数据（HEX）  | GB18030 to UTF8      | Unicode 4.1映射        |
   |------------------------|----------------------|------------------------|
   | 0x31903295             |       U+E816         |		\u20087         |
   | 0x33903295             |       U+E817         |		\u20089         |
   | 0x30973295             |       U+E818         |		\u200CC         |
   | 0x37B93695             |       U+E831         |		\u215D7         |
   | 0x35BA3096             |       U+E83B         |		\u2298F         |
   | 0x30B63596             |       U+E855         |		\u241FE         |
 */
bool checkGB18030ToUtf8Error(char *buf, size_t size, size_t &replaceLen, QByteArray &appendChar)
{
    qDebug() << "checkGB18030ToUtf8Error() entry, size:" << size;
    // 对于GB18030-2005编码规范PUA区域的字符，iconv报错，替换为对应的字符序。
    static const int sc_minGB18030PUACharLen = 4;
    if (size < sc_minGB18030PUACharLen) {
        qDebug() << "Size is less than min GB18030 PUA char length, returning false";
        replaceLen = 1;
        appendChar = "?";
        return false;
    }

    quint32 puaChar = *reinterpret_cast<quint32 *>(buf);
    qDebug() << "Processing puaChar:" << QString::number(puaChar, 16);
    appendChar = gs_UTF8MapGB18030Data.key(puaChar);
    if (appendChar.isEmpty()) {
        qDebug() << "appendChar from gs_UTF8MapGB18030Data is empty, checking gs_ReplaceToUTF8_2005Error";
        // FFFFFF0X -> \uE816...\uE855
        appendChar = gs_ReplaceToUTF8_2005Error.key(QByteArray(buf, sc_minGB18030PUACharLen));
    }
    qDebug() << "appendChar after first check:" << appendChar;

    if (appendChar.isEmpty()) {
        qDebug() << "appendChar is still empty, returning false";
        replaceLen = 1;
        appendChar = "?";
        return false;
    } else {
        qDebug() << "appendChar found, returning true, replaceLen:" << sc_minGB18030PUACharLen;
        replaceLen = sc_minGB18030PUACharLen;
        return true;
    }
}

/**
   @brief 检测 UTF-8 转换 GB18030 时PUA区域异常处理
 */
bool checkUTF8ToGB18030Error(char *buf, size_t size, size_t &replaceLen, QByteArray &appendChar)
{
    qDebug() << "checkUTF8ToGB18030Error() entry, size:" << size;
    // 待转换的PUA字符均为3
    static const int sc_minUTFPUACharLen = 3;
    if (size < sc_minUTFPUACharLen) {
        qDebug() << "Size is less than min UTF PUA char length, returning false";
        replaceLen = 1;
        appendChar = "?";
        return false;
    }

    QByteArray puaChar(buf, sc_minUTFPUACharLen);
    qDebug() << "Processing puaChar:" << puaChar.toHex();
    quint32 gb18030char = gs_UTF8MapGB18030Data.value(puaChar, 0);
    qDebug() << "gb18030char from gs_UTF8MapGB18030Data:" << QString::number(gb18030char, 16);
    if (!gb18030char) {
        qDebug() << "gb18030char is null, checking gs_ReplaceUtf8ToGB18030_2005Error";
        // \uE816 -> 0xFE51
        appendChar = gs_ReplaceUtf8ToGB18030_2005Error.value(puaChar);
        if (appendChar.isEmpty()) {
            qDebug() << "appendChar from gs_ReplaceUtf8ToGB18030_2005Error is empty, checking gs_ReplaceFromUtf8_2020Error";
            // 0xFFFF11 -> 0x95329031
            appendChar = gs_ReplaceFromUtf8_2020Error.key(puaChar);
        }
        qDebug() << "appendChar after second check:" << appendChar;

        if (!appendChar.isEmpty()) {
            qDebug() << "appendChar found, returning true, replaceLen:" << sc_minUTFPUACharLen;
            replaceLen = sc_minUTFPUACharLen;
            return true;
        }

        qDebug() << "appendChar is empty, returning false";
        replaceLen = 1;
        appendChar = "?";
        return false;
    } else {
        qDebug() << "gb18030char found, returning true, replaceLen:" << sc_minUTFPUACharLen;
        replaceLen = sc_minUTFPUACharLen;
        appendChar = QByteArray(reinterpret_cast<char *>(&gb18030char), sizeof(gb18030char));
        return true;
    }
}

/**
 * @brief 将输入的字符序列 \a inputStr 从编码 \a fromCode 转换为编码 \a toCode, 并返回转换后的字符序列。
 * @return 字符编码转换是否成功
 */
bool DetectCode::ChangeFileEncodingFormat(QByteArray &inputStr,
                                          QByteArray &outStr,
                                          const QString &fromCode,
                                          const QString &toCode)
{
    qDebug() << "[DetectCode] Enter ChangeFileEncodingFormat, from:" << fromCode << "to:" << toCode
             << "input size:" << inputStr.size();

    if (fromCode == toCode) {
        qDebug() << "fromCode and toCode are the same, no conversion needed";
        outStr = inputStr;
        return true;
    }

    if (inputStr.isEmpty()) {
        qDebug() << "Input string is empty, clearing output and returning true";
        outStr.clear();
        return true;
    }

#ifndef DISABLE_TEXTCODEC
    // 使用QTextCodec对部分编码进行处理
    static QStringList codecList{"GB18030"};
    if (codecList.contains(fromCode) || codecList.contains(toCode)) {
        qDebug() << "Codec list contains fromCode or toCode, using convertEncodingTextCodec";
        return convertEncodingTextCodec(inputStr, outStr, fromCode, toCode);
    }
#endif

    iconv_t handle = iconv_open(toCode.toLocal8Bit().data(), fromCode.toLocal8Bit().data());
    if (handle != reinterpret_cast<iconv_t>(-1)) {
        qDebug() << "iconv_open successful";
        MibEncoding fromMib = UnknownMib;
        QTextCodec *fromCodec = QTextCodec::codecForName(fromCode.toUtf8());
        if (fromCodec) {
            fromMib = static_cast<MibEncoding>(fromCodec->mibEnum());
            qDebug() << "fromMib:" << fromMib;
        }
        // 不使用上层修改的Iconv处理时，不检测转换的编码格式，跳过GB18030转换特殊处理
        MibEncoding toMib = UnknownMib;

        const bool enablePatchedIconv = Config::instance()->enablePatchedIconv();
        if (enablePatchedIconv) {
            qDebug() << "Patched Iconv enabled";
            QTextCodec *toCodec = QTextCodec::codecForName(toCode.toUtf8());
            if (toCodec) {
                toMib = static_cast<MibEncoding>(toCodec->mibEnum());
                qDebug() << "toMib:" << toMib;
            }

            // 替换需手动修改 UTF-8 和 GB180303 编码转换的部分
            if (GB18030 == fromMib && UTF_8 == toMib) {
                qDebug() << "Converting from GB18030 to UTF-8, applying 2005 error replacements";
                for (auto itr = gs_ReplaceFromGB18030_2005Error.begin(); itr != gs_ReplaceFromGB18030_2005Error.end(); ++itr) {
                    inputStr.replace(itr.key(), itr.value());
                }
            } else if (UTF_8 == fromMib && GB18030 == toMib) {
                qDebug() << "Converting from UTF-8 to GB18030, applying 2020 error replacements";
                for (auto itr = gs_ReplaceToGB18030_2020Error.begin(); itr != gs_ReplaceToGB18030_2020Error.end(); ++itr) {
                    inputStr.replace(itr.key(), itr.value());
                }
            }
        }

        char *inbuf = inputStr.data();
        size_t inbytesleft = static_cast<size_t>(inputStr.size());
        size_t outbytesleft = 4 * inbytesleft;
        char *outbuf = new char[outbytesleft];
        char *bufferHeader = outbuf;
        size_t maxBufferSize = outbytesleft;

        memset(outbuf, 0, outbytesleft);

        int errorNum = 0;
        try {
            size_t ret = 0;
            do {
                ret = iconv(handle, &inbuf, &inbytesleft, &outbuf, &outbytesleft);
                if (static_cast<size_t>(-1) == ret) {
                    qWarning() << "iconv conversion failed, errno:" << errno;
                    // 记录错误信息
                    errorNum = errno;

                    // 遇到错误的输入，错误码 EILSEQ (84)，跳过当前位置并添加'?'
                    if (EILSEQ == errorNum) {
                        qDebug() << "EILSEQ error encountered";
                        // 缓冲区不足跳出
                        if (outbytesleft == 0) {
                            qWarning() << "Output buffer exhausted, breaking conversion";
                            break;
                        }

                        size_t replaceLen = 1;
                        // 跳过错误字符，设置错误字符为'?'
                        QByteArray appendChar = "?";

                        switch (fromMib) {
                            case UTF_8: {
                                qDebug() << "UTF-8 source encoding detected";
                                // 特殊处理，若为UTF-8 到 GB18030的转换，优先排查异常数据
                                if (GB18030 == toMib) {
                                    qDebug() << "Target is GB18030, checking for UTF8ToGB18030Error";
                                    if (checkUTF8ToGB18030Error(inbuf, inbytesleft, replaceLen, appendChar)) {
                                        break;
                                    }
                                }

                                // 源编码为 UTF-8 时，可计算需跳过的字符数
                                replaceLen = static_cast<size_t>(utf8MultiByteCount(inbuf, inbytesleft));
                                qDebug() << "Calculated replaceLen for UTF-8:" << replaceLen;
                            } break;
                            case GB18030:
                                qDebug() << "GB18030 source encoding detected";
                                // 特殊处理，若为GB18030 到 UTF-8 的转换，优先排查异常数据
                                if (UTF_8 == toMib) {
                                    qDebug() << "Target is UTF-8, checking for GB18030ToUtf8Error";
                                    if (checkGB18030ToUtf8Error(inbuf, inbytesleft, replaceLen, appendChar)) {
                                        break;
                                    }
                                }
                                break;
                            default:
                                qDebug() << "Default case for fromMib, no special handling";
                                break;
                        }

                        // 替换错误字符为对应字符
                        size_t appendSize = static_cast<size_t>(appendChar.size());
                        if (outbytesleft < appendSize) {
                            qWarning() << "Output buffer too small for appendChar, breaking";
                            break;
                        }

                        outbytesleft -= appendSize;
                        ::memcpy(outbuf, appendChar.data(), appendSize);
                        outbuf += appendSize;
                        qDebug() << "Appended error character:" << appendChar << ", remaining outbytesleft:" << outbytesleft;

                        if (inbytesleft <= replaceLen) {
                            qDebug() << "No more input bytes left or replaceLen covers remaining, moving to end";
                            // 移动至末尾
                            inbuf += inbytesleft;
                            inbytesleft = 0;
                            break;
                        }

                        inbuf += replaceLen;
                        inbytesleft -= replaceLen;
                        qDebug() << "Skipped" << replaceLen << "bytes from input, remaining inbytesleft:" << inbytesleft;
                    } else {
                        qWarning() << "[DetectCode] iconv_open failed for conversion from" << fromCode << "to" << toCode;
                        break;
                    }
                }
            } while (static_cast<size_t>(-1) == ret);

        } catch (const std::exception &e) {
            qWarning() << qPrintable("iconv convert encoding catching exception") << qPrintable(e.what());
        }

        if (errorNum) {
            qWarning() << qPrintable("iconv() convert text encoding error, errocode:") << errorNum;
        }
        iconv_close(handle);

        // 手动添加 UTF BOM 信息
        outStr.append(gs_byteOrderMark.value(toCode));
        qDebug() << "Appended BOM for toCode:" << toCode;

        // 计算 iconv() 实际转换的字节数
        size_t realConvertSize = maxBufferSize - outbytesleft;
        outStr += QByteArray(bufferHeader, static_cast<int>(realConvertSize));
        qDebug() << "Converted data size:" << realConvertSize;

        delete[] bufferHeader;
        bufferHeader = nullptr;

        qDebug() << "Exit convertEncodingTextCodec, output size:" << outStr.size();

        return true;

    } else {
        qWarning() << qPrintable("Text encoding convert error, iconv_open() failed.");
        // 尝试使用QTextCodec加载
        return convertEncodingTextCodec(inputStr, outStr, fromCode, toCode);
    }
}

/**
 * @brief 使用QTextCodec将输入的字符序列 \a inputStr 从编码 \a fromCode 转换为编码 \a toCode, 并返回转换后的字符序列。
 * @return 字符编码转换是否成功
 */
bool DetectCode::convertEncodingTextCodec(QByteArray &inputStr,
                                          QByteArray &outStr,
                                          const QString &fromCode,
                                          const QString &toCode)
{
    qDebug() << "Enter convertEncodingTextCodec, from:" << fromCode << "to:" << toCode
             << "input size:" << inputStr.size();

    QString convertData;
    if (fromCode != "UTF-8") {
        qDebug() << "fromCode is not UTF-8";
        QTextCodec *fromCodec = QTextCodec::codecForName(fromCode.toUtf8());
        if (!fromCodec) {
            qWarning() << "Failed to get codec for fromCode:" << fromCode;
            return false;
        }

        convertData = fromCodec->toUnicode(inputStr);
    } else {
        qDebug() << "fromCode is UTF-8";
        convertData = QString::fromUtf8(inputStr);
    }

    if (toCode != "UTF-8") {
        qDebug() << "toCode is not UTF-8";
        QTextCodec *toCodec = QTextCodec::codecForName(toCode.toUtf8());
        if (!toCodec) {
            qWarning() << "Failed to get codec for toCode:" << toCode;
            return false;
        }

        outStr = toCodec->fromUnicode(convertData);
    } else {
        qDebug() << "toCode is UTF-8";
        outStr = convertData.toUtf8();
    }

    // 手动添加 UTF BOM 信息
    outStr.append(gs_byteOrderMark.value(toCode));
    qDebug() << "Exit convertEncodingTextCodec, output size:" << outStr.size();
    return true;
}
