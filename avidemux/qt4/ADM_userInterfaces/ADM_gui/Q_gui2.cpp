/***************************************************************************
    copyright            : (C) 2001 by mean
    email                : fixounet@free.fr
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <QColor>
#include <QGraphicsView>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMessageBox>
#include <QPalette>
#include <QResizeEvent>
#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QLocale>
#include <QtCore/QMimeData>
#include <QtCore/QUrl>
#if QT_VERSION < QT_VERSION_CHECK(5, 11, 0)
#include <QDesktopWidget>
#else
#include <QScreen>
#endif
#include <QClipboard>
#include <QFileDialog>
#ifdef USE_CUSTOM_TIME_DISPLAY_FONT
#include <QFontDatabase>
#endif
#include <QStyleFactory>

#ifdef __APPLE__
#include <QFileOpenEvent>
#endif

#include "ADM_cpp.h"
#define MENU_DECLARE
#include "Q_gui2.h"

#ifdef BROKEN_PALETTE_PROPAGATION
#include <QAbstractItemView>
#endif

#include "ADM_QSettings.h"
#include "ADM_default.h"
#include "ADM_qtx.h"
#include "ADM_toolkitQt.h"

#include "ADM_last.h"
#include "ADM_vidMisc.h"
#include "DIA_fileSel.h"
#include "avi_vars.h"
#include "prefs.h"

#include "../../ADM_update/include/ADM_update.h"
#include "ADM_coreVideoEncoderInternal.h"
#include "ADM_coreVideoFilter.h"
#include "ADM_muxerProto.h"
#include "ADM_confCouple.h"
#include "audioEncoderApi.h"
#include "ADM_preview.h"
#include "ADM_systemTrayProgress.h"
#include "DIA_coreToolkit.h"
#include "DIA_defaultAskAvisynthPort.hxx"
#include "GUI_render.h"
#include "GUI_renderInternal.h"
#include "GUI_ui.h"
#include "T_vumeter.h"
#include "config.h"
using namespace std;

#define ADM_SLIDER_REFRESH_PERIOD 500
#define ADM_LARGE_SCALE (10LL * 1000LL)
#define ADM_SCALE_INCREMENT (ADM_LARGE_SCALE / 100LL)
// if the dimensions of an autozoomed video are smaller than the available space,
// don't adjust zoom unless the window has been enlarged beyond a threshold
#define RESIZE_THRESHOLD 20

#define ADM_QT_THEME_DEFAULT 0
#define ADM_QT_THEME_LIGHT 1
#define ADM_QT_THEME_DARK 2

#if defined(USE_SDL) && (!defined(_WIN32) && !defined(__APPLE__))
#define SDL_ON_LINUX
#endif

#ifdef USE_OPENGL
extern bool ADM_glHasActiveTexture(void);
void UI_Qt4InitGl(void);
void UI_Qt4CleanGl(void);
bool openGLStarted = false;
#endif

MainWindow *MainWindow::mainWindowSingleton = NULL;
QApplication *currentQApplication();

extern int global_argc;
extern char **global_argv;

extern int automation(void);
extern void sendAction(Action a);
extern int encoderGetEncoderCount(void);
extern const char *encoderGetIndexedName(uint32_t i);
uint32_t audioEncoderGetNumberOfEncoders(void);
const char *audioEncoderGetDisplayName(int i);
extern void checkCrashFile(void);
extern bool A_checkSavedSession(bool load);
extern void UI_QT4VideoWidget(QFrame *frame);
extern void loadTranslator(void);
extern void initTranslator(void);
extern void destroyTranslator(void);
extern ADM_RENDER_TYPE UI_getPreferredRender(void);
extern int A_openVideo(const char *name);
extern int A_appendVideo(const char *name);
extern int videoEncoder6_GetIndexFromName(const char *name);
extern bool videoEncoder6_SetCurrentEncoder(uint32_t index);
extern bool videoEncoder6_SetProfile(const char *profile);
extern const char *videoEncoder6_GetCurrentEncoderName(void);
extern uint32_t ADM_vf_getTagFromInternalName(const char *name);
extern uint32_t ADM_vf_getSize(void);
extern uint32_t ADM_vf_getTag(int index);
extern bool ADM_vf_removeFilterAtIndex(int index);
extern ADM_coreVideoFilter *ADM_vf_getInstance(int index);
extern int A_SaveWrapper(const char *name);
int UI_getCurrentVCodec(void);

int SliderIsShifted = 0;
static void setupMenus(void);
static int shiftKeyHeld = 0;
static int ctrlKeyHeld = 0;

typedef enum
{
    ADM_PROFILE_SIZE_ORIGINAL = 0,
    ADM_PROFILE_SIZE_PROFILE_FIT = 1,
    ADM_PROFILE_SIZE_CUSTOM_FIT = 2,
    ADM_PROFILE_SIZE_CUSTOM_STRETCH = 3
} admProfileSizeMode;

typedef struct
{
    std::string label;
    std::string videoEncoder;
    std::string videoProfile;
    std::string audioEncoder;
    std::string fallbackAudioEncoder;
    std::string container;
    uint32_t targetWidth;
    uint32_t targetHeight;
    admProfileSizeMode defaultSizeMode;
    bool custom;
} admOutputProfile;

static const admOutputProfile admBuiltinOutputProfiles[] = {
    {"Copy", "", "", "copy", "", "MP4", 0, 0, ADM_PROFILE_SIZE_ORIGINAL, false},
    {"DivX HEVC 1080p", "x265", "DivX HEVC 1080p", "LavAAC", "copy", "MKV", 1920, 1080, ADM_PROFILE_SIZE_PROFILE_FIT, false},
    {"DivX HEVC 720p", "x265", "DivX HEVC 720p", "LavAAC", "copy", "MKV", 1280, 720, ADM_PROFILE_SIZE_PROFILE_FIT, false},
    {"DivX Plus 4K", "x264", "DivX Plus 4K", "LavAAC", "copy", "MKV", 3840, 2160, ADM_PROFILE_SIZE_PROFILE_FIT, false},
    {"DivX Plus HD", "x264", "DivX Plus HD", "LavAAC", "copy", "MKV", 1920, 1080, ADM_PROFILE_SIZE_PROFILE_FIT, false},
    {"MP4 iPad", "x264", "MP4 iPad", "LavAAC", "copy", "MP4", 1280, 720, ADM_PROFILE_SIZE_PROFILE_FIT, false},
    {"MP4 iPhone", "x264", "MP4 iPhone", "LavAAC", "copy", "MP4", 640, 480, ADM_PROFILE_SIZE_PROFILE_FIT, false},
};
static std::vector<admOutputProfile> admOutputProfiles;

static void admLoadCustomOutputProfiles(void)
{
    admOutputProfiles.clear();
    for (uint32_t i = 0; i < sizeof(admBuiltinOutputProfiles) / sizeof(admOutputProfile); i++)
        admOutputProfiles.push_back(admBuiltinOutputProfiles[i]);

    QSettings *qset = qtSettingsCreate();
    if (!qset)
        return;

    int count = qset->beginReadArray("customOutputProfiles");
    for (int i = 0; i < count; i++)
    {
        qset->setArrayIndex(i);
        QString label = qset->value("label").toString().trimmed();
        if (label.isEmpty())
            continue;
        admOutputProfile profile;
        profile.label = label.toUtf8().constData();
        profile.videoEncoder = qset->value("videoEncoder").toString().toUtf8().constData();
        profile.videoProfile = qset->value("videoProfile").toString().toUtf8().constData();
        profile.audioEncoder = qset->value("audioEncoder", "copy").toString().toUtf8().constData();
        profile.fallbackAudioEncoder = qset->value("fallbackAudioEncoder", "copy").toString().toUtf8().constData();
        profile.container = qset->value("container", "MP4").toString().toUtf8().constData();
        profile.targetWidth = qset->value("targetWidth", 0).toUInt();
        profile.targetHeight = qset->value("targetHeight", 0).toUInt();
        int mode = qset->value("sizeMode", (int)ADM_PROFILE_SIZE_ORIGINAL).toInt();
        if (mode < ADM_PROFILE_SIZE_ORIGINAL || mode > ADM_PROFILE_SIZE_CUSTOM_STRETCH)
            mode = ADM_PROFILE_SIZE_ORIGINAL;
        profile.defaultSizeMode = (admProfileSizeMode)mode;
        profile.custom = true;
        admOutputProfiles.push_back(profile);
    }
    qset->endArray();
    delete qset;
}

static void admStoreCustomOutputProfiles(void)
{
    QSettings *qset = qtSettingsCreate();
    if (!qset)
        return;

    qset->beginWriteArray("customOutputProfiles");
    int customIndex = 0;
    for (uint32_t i = 0; i < admOutputProfiles.size(); i++)
    {
        const admOutputProfile &profile = admOutputProfiles[i];
        if (!profile.custom)
            continue;
        qset->setArrayIndex(customIndex++);
        qset->setValue("label", QString::fromUtf8(profile.label.c_str()));
        qset->setValue("videoEncoder", QString::fromUtf8(profile.videoEncoder.c_str()));
        qset->setValue("videoProfile", QString::fromUtf8(profile.videoProfile.c_str()));
        qset->setValue("audioEncoder", QString::fromUtf8(profile.audioEncoder.c_str()));
        qset->setValue("fallbackAudioEncoder", QString::fromUtf8(profile.fallbackAudioEncoder.c_str()));
        qset->setValue("container", QString::fromUtf8(profile.container.c_str()));
        qset->setValue("targetWidth", profile.targetWidth);
        qset->setValue("targetHeight", profile.targetHeight);
        qset->setValue("sizeMode", (int)profile.defaultSizeMode);
    }
    qset->endArray();
    qset->sync();
    delete qset;
}

static void admRoundDownEven(uint32_t &value)
{
    if (value < 2)
        value = 2;
    value &= ~1U;
    if (value < 2)
        value = 2;
}

static void admFitDimensions(uint32_t sourceWidth, uint32_t sourceHeight, uint32_t boxWidth, uint32_t boxHeight, uint32_t &outWidth, uint32_t &outHeight)
{
    if (!sourceWidth || !sourceHeight || !boxWidth || !boxHeight)
    {
        outWidth = sourceWidth;
        outHeight = sourceHeight;
        return;
    }
    if ((uint64_t)sourceWidth * boxHeight > (uint64_t)boxWidth * sourceHeight)
    {
        outWidth = boxWidth;
        outHeight = (uint32_t)(((uint64_t)boxWidth * sourceHeight + sourceWidth / 2) / sourceWidth);
    }
    else
    {
        outHeight = boxHeight;
        outWidth = (uint32_t)(((uint64_t)boxHeight * sourceWidth + sourceHeight / 2) / sourceHeight);
    }
    admRoundDownEven(outWidth);
    admRoundDownEven(outHeight);
}

static void admRemoveSwscaleResizeFilters(void)
{
    uint32_t swscaleTag = ADM_vf_getTagFromInternalName("swscale");
    if (swscaleTag == (uint32_t)-1)
        return;
    for (int i = (int)ADM_vf_getSize() - 1; i >= 0; i--)
    {
        if (ADM_vf_getTag(i) == swscaleTag)
            ADM_vf_removeFilterAtIndex(i);
    }
}

static bool admApplyOutputProfileResize(admProfileSizeMode mode, uint32_t targetWidth, uint32_t targetHeight)
{
    if (!video_body)
        return false;

    admRemoveSwscaleResizeFilters();
    if (mode == ADM_PROFILE_SIZE_ORIGINAL)
        return true;

    aviInfo info;
    if (!video_body->getVideoInfo(&info) || !info.width || !info.height)
    {
        ADM_warning("Output profile: no video loaded, resize will be skipped\n");
        return false;
    }

    uint32_t outputWidth = targetWidth;
    uint32_t outputHeight = targetHeight;
    if (!outputWidth || !outputHeight)
        return false;

    bool keepAspectRatio = (mode != ADM_PROFILE_SIZE_CUSTOM_STRETCH);
    if (keepAspectRatio)
        admFitDimensions(info.width, info.height, targetWidth, targetHeight, outputWidth, outputHeight);
    else
    {
        admRoundDownEven(outputWidth);
        admRoundDownEven(outputHeight);
    }

    if (outputWidth == info.width && outputHeight == info.height)
    {
        ADM_info("Output profile: resize skipped, output already %" PRIu32 "x%" PRIu32 "\n", outputWidth, outputHeight);
        return true;
    }

    CONFcouple *couples = new CONFcouple(7);
    couples->writeAsUint32("width", outputWidth);
    couples->writeAsUint32("height", outputHeight);
    couples->writeAsUint32("algo", 1);
    couples->writeAsUint32("sourceAR", 0);
    couples->writeAsUint32("targetAR", 0);
    couples->writeAsBool("lockAR", keepAspectRatio);
    couples->writeAsUint32("roundup", 0);

    if (!video_body->addVideoFilter("swscale", couples))
    {
        ADM_warning("Output profile: failed to add swscale resize filter %" PRIu32 "x%" PRIu32 "\n", outputWidth, outputHeight);
        return false;
    }
    ADM_info("Output profile: swscale resize set to %" PRIu32 "x%" PRIu32 "%s\n",
             outputWidth, outputHeight, keepAspectRatio ? " (keep aspect ratio)" : " (stretch)");
    return true;
}

static int admFindVideoEncoderIndex(const char *encoderName)
{
    if (!encoderName)
        return 0;
    int index = videoEncoder6_GetIndexFromName(encoderName);
    if (index < 0)
        ADM_warning("Output profile: video encoder \"%s\" not found\n", encoderName);
    return index;
}

static bool admSetProfileAudioEncoder(const char *encoderName, const char *fallbackEncoderName)
{
    if (!encoderName || !strcasecmp(encoderName, "copy"))
    {
        UI_setAudioCodec(0);
        return audioCodecSetByIndex(0, 0);
    }
    if (audioCodecSetByName(0, encoderName))
        return true;

    ADM_warning("Output profile: audio encoder \"%s\" not found\n", encoderName);
    if (fallbackEncoderName && strcasecmp(fallbackEncoderName, encoderName))
    {
        ADM_warning("Output profile: trying fallback audio encoder \"%s\"\n", fallbackEncoderName);
        if (!strcasecmp(fallbackEncoderName, "copy"))
        {
            UI_setAudioCodec(0);
            return audioCodecSetByIndex(0, 0);
        }
        return !!audioCodecSetByName(0, fallbackEncoderName);
    }
    return false;
}

static void admApplyOutputProfile(int profileIndex)
{
    int nbProfiles = (int)admOutputProfiles.size();
    if (profileIndex < 0 || profileIndex >= nbProfiles)
        return;

    const admOutputProfile &profile = admOutputProfiles[profileIndex];
    ADM_info("Applying output profile \"%s\"\n", profile.label.c_str());

    int videoIndex = admFindVideoEncoderIndex(profile.videoEncoder.empty() ? NULL : profile.videoEncoder.c_str());
    if (videoIndex >= 0)
    {
        UI_setVideoCodec(videoIndex);
        videoEncoder6_SetCurrentEncoder(videoIndex);
        if (!profile.videoProfile.empty() && !videoEncoder6_SetProfile(profile.videoProfile.c_str()))
            ADM_warning("Output profile: failed to load video profile \"%s\"\n", profile.videoProfile.c_str());
    }

    admSetProfileAudioEncoder(profile.audioEncoder.empty() ? NULL : profile.audioEncoder.c_str(),
                              profile.fallbackAudioEncoder.empty() ? NULL : profile.fallbackAudioEncoder.c_str());

    if (!profile.container.empty())
    {
        int containerIndex = ADM_MuxerIndexFromName(profile.container.c_str());
        if (containerIndex >= 0)
            UI_SetCurrentFormat((uint32_t)containerIndex);
        else
            ADM_warning("Output profile: container \"%s\" not found\n", profile.container.c_str());
    }
}
static ADM_mwNavSlider *slider = NULL;
static int _upd_in_progres = 0;
bool ADM_ve6_getEncoderInfo(int filter, const char **name, uint32_t *major, uint32_t *minor, uint32_t *patch);
uint32_t ADM_ve6_getNbEncoders(void);
void UI_refreshCustomMenu(void);
admUITaskBarProgress *QuiTaskBarProgress;
extern admUITaskBarProgress *createADMTaskBarProgress();
QWidget *QuiMainWindows = NULL;
QWidget *VuMeter = NULL;
QGraphicsView *drawWindow = NULL;

extern void saveCrashProject(void);
extern uint8_t AVDM_setVolume(int volume);
extern bool AVDM_hasVolumeControl(void);
extern bool ADM_QPreviewCleanup(void);
extern void vdpauCleanup();
extern bool A_loadDefaultSettings(void);

extern int ADM_clearQtShellHistory(void);
extern void ADM_ExitCleanup(void);

#ifdef _WIN32
static void UI_centerMainWindowOnScreen(void)
{
    if (!QuiMainWindows || QuiMainWindows->isMaximized())
        return;

#if QT_VERSION < QT_VERSION_CHECK(5, 11, 0)
    QRect space = QApplication::desktop()->availableGeometry(QuiMainWindows);
#else
    QScreen *screen = QGuiApplication::screenAt(QuiMainWindows->frameGeometry().center());
    if (!screen)
        screen = QGuiApplication::primaryScreen();
    QRect space = screen->availableGeometry();
#endif
    if (!space.isValid())
        return;

    QRect fs = QuiMainWindows->frameGeometry();
    int x = space.x() + (space.width() - fs.width()) / 2;
    int y = space.y() + (space.height() - fs.height()) / 2;

    if (x < space.x())
        x = space.x();
    if (y < space.y())
        y = space.y();

    ADM_info("Moving the main window to centered position (%d, %d)\n", x, y);
    QuiMainWindows->move(x, y);
}
#endif

static bool uiRunning = false;
static bool uiIsMaximized = false;

static bool needsResizing = false;

static QAction *findAction(std::vector<MenuEntry> *list, Action action);
static QAction *findActionInToolBar(QToolBar *tb, Action action);

#define WIDGET(x) (((MainWindow *)QuiMainWindows)->ui.x)

#define CONNECT(object, zzz) connect((ui.object), SIGNAL(triggered()), this, SLOT(buttonPressed()));
#define CONNECT_TB(object, zzz) connect((ui.object), SIGNAL(clicked(bool)), this, SLOT(toolButtonPressed(bool)));
#define DECLARE_VAR(object, signal_name) {#object, signal_name},

typedef enum
{
    ADM_CUSTOM_LANG_EN = 0,
    ADM_CUSTOM_LANG_ZH_CN,
    ADM_CUSTOM_LANG_ZH_TW
} admCustomUiLang;

static admCustomUiLang admGetCustomUiLang(void)
{
    std::string configuredLanguage;
    QString locale;
    if (prefs && prefs->get(DEFAULT_LANGUAGE, configuredLanguage) && configuredLanguage.size() && configuredLanguage != "auto")
        locale = QString::fromUtf8(configuredLanguage.c_str()).toLower();
    else
        locale = QLocale::system().name().toLower();
    if (locale.startsWith("zh_tw") || locale.startsWith("zh_hk") || locale.startsWith("zh_mo") || locale.startsWith("zh_hant"))
        return ADM_CUSTOM_LANG_ZH_TW;
    if (locale.startsWith("zh"))
        return ADM_CUSTOM_LANG_ZH_CN;
    return ADM_CUSTOM_LANG_EN;
}

static QString admUiText(const char *en, const char *zhCN, const char *zhTW)
{
    switch (admGetCustomUiLang())
    {
    case ADM_CUSTOM_LANG_ZH_CN:
        return QString::fromUtf8(zhCN);
    case ADM_CUSTOM_LANG_ZH_TW:
        return QString::fromUtf8(zhTW);
    default:
        return QString::fromUtf8(en);
    }
}

static QString admCustomProfilePrefix(void)
{
    return admUiText("Custom: ", "自定义：", "自訂：");
}

static void admApplyCustomUiTranslations(Ui_MainWindow &ui)
{
    ui.labelProfile->setText(QString::fromUtf8("<b>%1</b>").arg(admUiText("Output Profile", "输出配置", "輸出設定")));
    ui.pushButtonProfileSave->setText(admUiText("Save", "保存", "儲存"));
    ui.labelProfileSize->setText(admUiText("Size", "尺寸", "尺寸"));
    ui.labelProfileResizeBy->setText(admUiText("by", "×", "×"));
    ui.checkBoxAutoSaveOutput->setText(admUiText("Auto save to folder", "自动保存到目录", "自動儲存到資料夾"));
    ui.lineEditAutoSaveOutputDir->setPlaceholderText(admUiText("Output folder", "输出目录", "輸出資料夾"));
}

static bool admProfileResizeSyncing = false;
static admProfileSizeMode admGetProfileResizeModeFromUi(void);

static void admSetProfileResizeControlsEnabled(admProfileSizeMode mode)
{
    bool customSize = (mode == ADM_PROFILE_SIZE_CUSTOM_FIT || mode == ADM_PROFILE_SIZE_CUSTOM_STRETCH);
    bool fixedProfileSize = (mode == ADM_PROFILE_SIZE_PROFILE_FIT);
    bool enabled = customSize || fixedProfileSize;
    WIDGET(spinBoxProfileWidth)->setEnabled(customSize);
    WIDGET(spinBoxProfileHeight)->setEnabled(customSize);
    WIDGET(labelProfileResizeBy)->setEnabled(enabled);
}

static bool admGetProfileAspectSource(uint32_t &sourceWidth, uint32_t &sourceHeight)
{
    sourceWidth = 0;
    sourceHeight = 0;

    if (video_body)
    {
        aviInfo info;
        if (video_body->getVideoInfo(&info) && info.width && info.height)
        {
            sourceWidth = info.width;
            sourceHeight = info.height;
            return true;
        }
    }

    int profileIndex = WIDGET(comboBoxProfile)->currentIndex();
    if (profileIndex >= 0 && profileIndex < (int)admOutputProfiles.size())
    {
        const admOutputProfile &profile = admOutputProfiles[profileIndex];
        if (profile.targetWidth && profile.targetHeight)
        {
            sourceWidth = profile.targetWidth;
            sourceHeight = profile.targetHeight;
            return true;
        }
    }

    uint32_t width = (uint32_t)WIDGET(spinBoxProfileWidth)->value();
    uint32_t height = (uint32_t)WIDGET(spinBoxProfileHeight)->value();
    if (width && height)
    {
        sourceWidth = width;
        sourceHeight = height;
        return true;
    }
    return false;
}

static void admSyncCustomProfileResizeFromSender(QObject *senderObject)
{
    if (admProfileResizeSyncing)
        return;
    if (admGetProfileResizeModeFromUi() != ADM_PROFILE_SIZE_CUSTOM_FIT)
        return;

    uint32_t sourceWidth = 0, sourceHeight = 0;
    if (!admGetProfileAspectSource(sourceWidth, sourceHeight))
        return;

    admProfileResizeSyncing = true;
    WIDGET(spinBoxProfileWidth)->blockSignals(true);
    WIDGET(spinBoxProfileHeight)->blockSignals(true);

    if (senderObject == WIDGET(spinBoxProfileWidth))
    {
        uint32_t width = (uint32_t)WIDGET(spinBoxProfileWidth)->value();
        uint32_t height = (uint32_t)(((uint64_t)width * sourceHeight + sourceWidth / 2) / sourceWidth);
        admRoundDownEven(height);
        WIDGET(spinBoxProfileHeight)->setValue((int)height);
    }
    else if (senderObject == WIDGET(spinBoxProfileHeight))
    {
        uint32_t height = (uint32_t)WIDGET(spinBoxProfileHeight)->value();
        uint32_t width = (uint32_t)(((uint64_t)height * sourceWidth + sourceHeight / 2) / sourceHeight);
        admRoundDownEven(width);
        WIDGET(spinBoxProfileWidth)->setValue((int)width);
    }

    WIDGET(spinBoxProfileWidth)->blockSignals(false);
    WIDGET(spinBoxProfileHeight)->blockSignals(false);
    admProfileResizeSyncing = false;
}

static admProfileSizeMode admGetProfileResizeModeFromUi(void)
{
    int mode = WIDGET(comboBoxProfileResizeMode)->currentIndex();
    if (mode < ADM_PROFILE_SIZE_ORIGINAL || mode > ADM_PROFILE_SIZE_CUSTOM_STRETCH)
        return ADM_PROFILE_SIZE_ORIGINAL;
    return (admProfileSizeMode)mode;
}

static void admSetProfileResizeUi(int profileIndex)
{
    int nbProfiles = (int)admOutputProfiles.size();
    if (profileIndex < 0 || profileIndex >= nbProfiles)
        return;

    const admOutputProfile &profile = admOutputProfiles[profileIndex];
    WIDGET(comboBoxProfileResizeMode)->blockSignals(true);
    WIDGET(spinBoxProfileWidth)->blockSignals(true);
    WIDGET(spinBoxProfileHeight)->blockSignals(true);

    WIDGET(comboBoxProfileResizeMode)->setCurrentIndex((int)profile.defaultSizeMode);
    if (profile.targetWidth && profile.targetHeight)
    {
        WIDGET(spinBoxProfileWidth)->setValue((int)profile.targetWidth);
        WIDGET(spinBoxProfileHeight)->setValue((int)profile.targetHeight);
    }

    WIDGET(comboBoxProfileResizeMode)->blockSignals(false);
    WIDGET(spinBoxProfileWidth)->blockSignals(false);
    WIDGET(spinBoxProfileHeight)->blockSignals(false);
    admSetProfileResizeControlsEnabled(profile.defaultSizeMode);
}

static void admResetProfileSizeToCurrentProfile(void)
{
    int profileIndex = WIDGET(comboBoxProfile)->currentIndex();
    int nbProfiles = (int)admOutputProfiles.size();
    if (profileIndex < 0 || profileIndex >= nbProfiles)
        return;
    const admOutputProfile &profile = admOutputProfiles[profileIndex];
    if (!profile.targetWidth || !profile.targetHeight)
        return;
    WIDGET(spinBoxProfileWidth)->blockSignals(true);
    WIDGET(spinBoxProfileHeight)->blockSignals(true);
    WIDGET(spinBoxProfileWidth)->setValue((int)profile.targetWidth);
    WIDGET(spinBoxProfileHeight)->setValue((int)profile.targetHeight);
    WIDGET(spinBoxProfileWidth)->blockSignals(false);
    WIDGET(spinBoxProfileHeight)->blockSignals(false);
}

static void admApplyProfileResizeFromUi(void)
{
    admProfileSizeMode mode = admGetProfileResizeModeFromUi();
    admSetProfileResizeControlsEnabled(mode);
    admApplyOutputProfileResize(mode,
                                (uint32_t)WIDGET(spinBoxProfileWidth)->value(),
                                (uint32_t)WIDGET(spinBoxProfileHeight)->value());
}

static uint32_t admGetCurrentOutputHeightForNaming(void)
{
    uint32_t nbFilters = ADM_vf_getSize();
    if (nbFilters)
    {
        ADM_coreVideoFilter *filter = ADM_vf_getInstance((int)nbFilters - 1);
        if (filter && filter->getInfo() && filter->getInfo()->height)
            return filter->getInfo()->height;
    }
    if (video_body)
    {
        aviInfo info;
        if (video_body->getVideoInfo(&info) && info.height)
            return info.height;
    }
    return 0;
}

static QString admBuildSequencedOutputPath(const QString &directory, const QString &baseName, const QString &stemSuffix,
                                           const QString &extension, bool suffixAlreadyNumbered)
{
    QString ext = extension;
    if (!ext.isEmpty() && !ext.startsWith("."))
        ext = "." + ext;

    QString prefix = QDir(directory).filePath(baseName + stemSuffix);
    if (!suffixAlreadyNumbered)
    {
        QString first = prefix + ext;
        if (!QFileInfo::exists(first))
            return first;
    }

    for (int i = 1; i < 10000; i++)
    {
        QString candidate = prefix + QString("-%1").arg(i, 3, 10, QChar('0')) + ext;
        if (!QFileInfo::exists(candidate))
            return candidate;
    }
    return prefix + QString("-%1").arg(10000) + ext;
}

static QString admCurrentVideoEncoderSuffixForNaming(void)
{
    const char *encoder = videoEncoder6_GetCurrentEncoderName();
    if (!encoder || !strlen(encoder))
        return QString::fromUtf8("encoded");

    if (!strcasecmp(encoder, "x264"))
        return QString::fromUtf8("h264");
    if (!strcasecmp(encoder, "x265"))
        return QString::fromUtf8("hevc");

    QString suffix = QString::fromUtf8(encoder).toLower();
    for (int i = 0; i < suffix.size(); i++)
    {
        if (!suffix[i].isLetterOrNumber())
            suffix[i] = QChar('-');
    }
    while (suffix.contains("--"))
        suffix.replace("--", "-");
    suffix = suffix.trimmed();
    while (suffix.startsWith("-"))
        suffix.remove(0, 1);
    while (suffix.endsWith("-"))
        suffix.chop(1);
    if (suffix.isEmpty())
        suffix = QString::fromUtf8("encoded");
    return suffix;
}

bool UI_buildSuggestedSavePath(const char *extension, const char *outputDir, char *target, uint32_t max)
{
    if (!target || max < 2)
        return false;

    std::string lastRead;
    admCoreUtils::getLastReadFile(lastRead);
    QString source = lastRead.size() ? QString::fromUtf8(lastRead.c_str()) : QString();
    QFileInfo sourceInfo(source);
    QString baseName = sourceInfo.exists() ? sourceInfo.completeBaseName() : QString::fromUtf8("out");

    QString directory = outputDir && strlen(outputDir) ? QString::fromUtf8(outputDir) : QString();
    if (directory.isEmpty() || !QDir(directory).exists())
        directory = sourceInfo.exists() ? sourceInfo.absolutePath() : QDir::homePath();
    if (directory.isEmpty() || !QDir(directory).exists())
        directory = QDir::homePath();

    QString ext = extension && strlen(extension) ? QString::fromUtf8(extension) : sourceInfo.suffix();
    bool copyMode = (UI_getCurrentVCodec() == 0);
    QString candidate;
    if (copyMode)
    {
        candidate = admBuildSequencedOutputPath(directory, baseName, QString(), ext, true);
    }
    else
    {
        uint32_t height = admGetCurrentOutputHeightForNaming();
        QString encoderSuffix = admCurrentVideoEncoderSuffixForNaming();
        QString suffix = height ? QString("-%1p-%2").arg(height).arg(encoderSuffix) : QString("-%1").arg(encoderSuffix);
        candidate = admBuildSequencedOutputPath(directory, baseName, suffix, ext, false);
    }

#ifdef _WIN32
    candidate = QDir::toNativeSeparators(candidate);
#endif
    QByteArray bytes = candidate.toUtf8();
    if ((uint32_t)bytes.size() >= max)
    {
        ADM_warning("Path length %d exceeds max %d\n", bytes.size(), max - 1);
        return false;
    }
    strncpy(target, bytes.constData(), max);
    target[max - 1] = 0;
    return true;
}

static int admFindCustomOutputProfileByLabel(const std::string &label)
{
    for (uint32_t i = 0; i < admOutputProfiles.size(); i++)
    {
        if (admOutputProfiles[i].custom && admOutputProfiles[i].label == label)
            return (int)i;
    }
    return -1;
}

static std::string admGetCurrentProfileVideoProfile(void)
{
    int profileIndex = WIDGET(comboBoxProfile)->currentIndex();
    if (profileIndex < 0 || profileIndex >= (int)admOutputProfiles.size())
        return "";

    const admOutputProfile &profile = admOutputProfiles[profileIndex];
    const char *currentVideoEncoder = videoEncoder6_GetCurrentEncoderName();
    if (!currentVideoEncoder)
        return "";
    if (!profile.videoEncoder.empty() && !strcasecmp(profile.videoEncoder.c_str(), currentVideoEncoder))
        return profile.videoProfile;
    return "";
}

static const char *admGetCurrentMuxerInternalName(void)
{
    int currentIndex = UI_GetCurrentFormat();
    static const char *knownMuxers[] = {"MP4", "MP4V2", "MKV", "WEBM", "AVI", "ffTS", "ffPS", "flv", "dummy"};
    for (uint32_t i = 0; i < sizeof(knownMuxers) / sizeof(const char *); i++)
    {
        if (ADM_MuxerIndexFromName(knownMuxers[i]) == currentIndex)
            return knownMuxers[i];
    }
    ADM_warning("Output profile: cannot map current muxer index %d to an internal name, falling back to MP4\n", currentIndex);
    return "MP4";
}

static void admRefreshOutputProfileCombo(int selectedIndex)
{
    WIDGET(comboBoxProfile)->blockSignals(true);
    WIDGET(comboBoxProfile)->clear();
    for (uint32_t i = 0; i < admOutputProfiles.size(); i++)
    {
        QString label = QString::fromUtf8(admOutputProfiles[i].label.c_str());
        if (admOutputProfiles[i].custom)
            label = admCustomProfilePrefix() + label;
        WIDGET(comboBoxProfile)->addItem(label);
    }
    if (selectedIndex >= 0 && selectedIndex < (int)admOutputProfiles.size())
        WIDGET(comboBoxProfile)->setCurrentIndex(selectedIndex);
    else
        WIDGET(comboBoxProfile)->setCurrentIndex(0);
    WIDGET(comboBoxProfile)->blockSignals(false);
}

static void admSaveCurrentOutputProfile(QWidget *parent)
{
    bool ok = false;
    QString defaultName = WIDGET(comboBoxProfile)->currentText();
    int currentIndex = WIDGET(comboBoxProfile)->currentIndex();
    if (currentIndex >= 0 && currentIndex < (int)admOutputProfiles.size() && admOutputProfiles[currentIndex].custom)
        defaultName = QString::fromUtf8(admOutputProfiles[currentIndex].label.c_str());
    QString name = QInputDialog::getText(parent, admUiText("Save Output Profile", "保存输出配置", "儲存輸出設定"),
                                         admUiText("Profile name:", "配置名称：", "設定名稱："), QLineEdit::Normal,
                                         defaultName, &ok);
    if (!ok)
        return;
    name = name.trimmed();
    if (name.isEmpty())
    {
        QMessageBox::warning(parent, admUiText("Save Output Profile", "保存输出配置", "儲存輸出設定"),
                             admUiText("Profile name cannot be empty.", "配置名称不能为空。", "設定名稱不能空白。"));
        return;
    }

    admOutputProfile profile;
    profile.label = name.toUtf8().constData();
    const char *videoEncoder = videoEncoder6_GetCurrentEncoderName();
    profile.videoEncoder = videoEncoder ? videoEncoder : "";
    profile.videoProfile = admGetCurrentProfileVideoProfile();
    const char *audioEncoder = NULL;
    if (WIDGET(comboBoxAudio)->currentIndex() == 0)
        audioEncoder = "copy";
    else
        audioEncoder = audioCodecGetName(0);
    profile.audioEncoder = audioEncoder ? audioEncoder : "copy";
    profile.fallbackAudioEncoder = "copy";
    profile.container = admGetCurrentMuxerInternalName();
    profile.targetWidth = (uint32_t)WIDGET(spinBoxProfileWidth)->value();
    profile.targetHeight = (uint32_t)WIDGET(spinBoxProfileHeight)->value();
    profile.defaultSizeMode = admGetProfileResizeModeFromUi();
    profile.custom = true;

    int index = admFindCustomOutputProfileByLabel(profile.label);
    if (index >= 0)
        admOutputProfiles[index] = profile;
    else
    {
        admOutputProfiles.push_back(profile);
        index = (int)admOutputProfiles.size() - 1;
    }

    admStoreCustomOutputProfiles();
    admRefreshOutputProfileCombo(index);
    admSetProfileResizeUi(index);
    QMessageBox::information(parent, admUiText("Save Output Profile", "保存输出配置", "儲存輸出設定"),
                             admUiText("Custom output profile saved.", "已保存自定义输出配置。", "已儲存自訂輸出設定。"));
}

#include "translation_table.h"
/*
    Declare the table converting widget name to our internal signal
*/
typedef struct
{
    const char *name;
    Action action;
} adm_qt4_translation;

adm_qt4_translation myTranslationTable[] = {
#define PROCESS DECLARE_VAR
    LIST_OF_BUTTONS
#undef PROCESS
};
static Action searchTranslationTable(const char *name);
static bool modifyTranslationTable(const char *name, Action a);
#define SIZEOF_MY_TRANSLATION sizeof(myTranslationTable) / sizeof(adm_qt4_translation)

class FileDropEvent : public QEvent
{
  public:
    QList<QUrl> files;

    FileDropEvent(QList<QUrl> files) : QEvent(QEvent::User)
    {
        this->files = files;
    }
};

#ifdef __APPLE__
/**
 *  \fn event
 *  \brief Queue requests from Finder to open files e.g. when they are dropped onto Avidemux icon in the dock
 */
bool myQApplication::event(QEvent *event)
{
    if (event->type() == QEvent::FileOpen)
    {
        QFileOpenEvent *openEvent = static_cast<QFileOpenEvent *>(event);
        ADM_info("FileOpen event for \"%s\"\n", openEvent->file().toUtf8().constData());
        fileOpenQueue.append(openEvent->url());
        handleFileOpenRequests();
    }
    return QApplication::event(event);
}
/**
 *  \fn handleFileOpenRequests
 */
void myQApplication::handleFileOpenRequests(void)
{
    if (QuiMainWindows && ready && fileOpenQueue.size())
    {
        MainWindow *mw = reinterpret_cast<MainWindow *>(QuiMainWindows);
        mw->fileOpenWrapper(fileOpenQueue);
        fileOpenQueue.clear();
    }
}
#endif
/**
 *
 *
 */
// #ifdef USING_QT6
// #include "oclero/qlementine.hpp"
// #endif
static void mySetStyle()
{
#ifdef USING_QT6
    // QApplication::setStyle(new oclero::qlementine::QlementineStyle(currentQApplication()));
    QApplication::setStyle("fusion");
#elif defined(USING_QT5)
    QApplication::setStyle("fusion");
#else
#error "QT4 is obsolete"
    QApplication::setStyle("cleanlooks");
#endif
}

void MainWindow::comboChanged(int z)
{
    QObject *obj = sender();

    if (obj == ui.comboBoxVideo)
        sendAction(ACT_VIDEO_CODEC_CHANGED);
    else if (obj == ui.comboBoxAudio)
        sendAction(ACT_AUDIO_CODEC_CHANGED);
    setMenuItemsEnabledState();
}

void MainWindow::profileChanged(int z)
{
    admSetProfileResizeUi(z);
    admApplyOutputProfile(z);
    admApplyProfileResizeFromUi();
    setMenuItemsEnabledState();
}

void MainWindow::profileResizeChanged(int z)
{
    if ((admProfileSizeMode)z == ADM_PROFILE_SIZE_PROFILE_FIT)
        admResetProfileSizeToCurrentProfile();
    admApplyProfileResizeFromUi();
    setMenuItemsEnabledState();
}

void MainWindow::profileResizeValueChanged(int z)
{
    UNUSED_ARG(z);
    admSyncCustomProfileResizeFromSender(sender());
    admApplyProfileResizeFromUi();
    setMenuItemsEnabledState();
}

void MainWindow::saveProfilePressed(void)
{
    admSaveCurrentOutputProfile(this);
    setMenuItemsEnabledState();
}

void MainWindow::autoSaveOutputToggled(bool checked)
{
    ui.lineEditAutoSaveOutputDir->setEnabled(checked);
    ui.pushButtonAutoSaveOutputBrowse->setEnabled(checked);
    QSettings *qset = qtSettingsCreate();
    if (qset)
    {
        qset->setValue("autoSaveOutput/enabled", checked);
        qset->sync();
        delete qset;
    }
}

void MainWindow::autoSaveOutputDirChanged(const QString &dir)
{
    QSettings *qset = qtSettingsCreate();
    if (qset)
    {
        qset->setValue("autoSaveOutput/dir", dir);
        qset->sync();
        delete qset;
    }
}

void MainWindow::autoSaveOutputBrowsePressed(void)
{
    QString start = ui.lineEditAutoSaveOutputDir->text();
    if (start.isEmpty() || !QDir(start).exists())
        start = QDir::homePath();
    QString dir = QFileDialog::getExistingDirectory(this, admUiText("Select auto save folder", "选择自动保存目录", "選擇自動儲存資料夾"),
                                                    start, QFileDialog::ShowDirsOnly);
    if (!dir.isEmpty())
        ui.lineEditAutoSaveOutputDir->setText(QDir::toNativeSeparators(dir));
}
/**
 * \fn sliderValueChanged
 * @param u
 */
void MainWindow::sliderValueChanged(int u)
{

    if (_upd_in_progres)
        return;
    if (refreshCapEnabled)
        switch (dragState)
        {
        default:
        case dragState_Normal:
            if (!dragWhilePlay && ctrlKeyHeld)
                sendAction(ACT_FineScale);
            else
                sendAction(ACT_Scale);
            break;
        case dragState_Active:
            dragTimer.stop();
            dragTimer.start(refreshCapValue);
            dragState = dragState_HoldOff;
            break;
        case dragState_HoldOff:
            break;
        }
    else if (!dragWhilePlay && ctrlKeyHeld)
        sendAction(ACT_FineScale);
    else
        sendAction(ACT_Scale);
}

/**
 *
 * @param version
 * @param date
 * @param url
 */
void MainWindow::updateAvailableSlot(int version, std::string date, std::string url)
{
    QMessageBox msgBox;
    int a, b, c;
    a = version / 10000;
    b = (version - a * 10000) / 100;
    c = version % 100;
    QString versionString = QString("%1.%2.%3").arg(a).arg(b).arg(c);
    QString msg = QT_TRANSLATE_NOOP(
        "qgui2", "<b>New version available</b><br> Version %1<br>Released on %2.<br>You can download it here<br> <a "
                 "href='%3'>%3</a><br><br><small> You can disable autoupdate in preferences.</small>");
    msg = msg.arg(versionString, date.c_str(), url.c_str());
    msgBox.setText(msg);
    msgBox.setTextFormat(Qt::RichText);
    msgBox.exec();
}
/**
 * \fn dragTimerTimeout
 */
void MainWindow::dragTimerTimeout(void)
{
    ADM_info("Drag timeout\n");
    switch (dragState)
    {
    default:
    case dragState_Normal:
    case dragState_Active:
        break;
    case dragState_HoldOff:
        dragState = dragState_Active;
        if (!dragWhilePlay && ctrlKeyHeld)
            sendAction(ACT_FineScale);
        else
            sendAction(ACT_Scale);
        break;
    }
}
/**
 * \fn sliderMoved
 */
void MainWindow::sliderMoved(int value)
{
    // ADM_info("Moved\n");
    SliderIsShifted = shiftKeyHeld;
}
/**
 * \fn sliderReleased
 */
void MainWindow::sliderReleased(void)
{
    // ADM_info("Released\n");
    SliderIsShifted = 0;
    dragTimer.stop();
    dragState = dragState_Normal;
    if (!dragWhilePlay && ctrlKeyHeld)
        sendAction(ACT_FineScale);
    else
        sendAction(ACT_Scale);
    if (dragWhilePlay)
        sendAction(ACT_PlayAvi); // resume playback
}
/**
 * \fn sliderPressed
 */
void MainWindow::sliderPressed(void)
{
    if (playing)
        sendAction(ACT_PlayAvi); // stop playback
    dragWhilePlay = playing;
    dragTimer.stop();
    dragState = dragState_Active;
    //  ADM_info("Pressed\n");
}
/**
 * \fn sliderWheel
 */
void MainWindow::sliderWheel(int way)
{
    bool swapWheel = false;
    prefs->get(FEATURES_SWAP_MOUSE_WHEEL, &swapWheel);
    if (swapWheel)
        way *= -1;
    if (way > 0)
    {
        if (ctrlKeyHeld)
            sendAction(ACT_NextFrame);
        else
            sendAction(ACT_NextKFrame);
        return;
    }
    if (way < 0)
    {
        if (ctrlKeyHeld)
            sendAction(ACT_PreviousFrame);
        else
            sendAction(ACT_PreviousKFrame);
    }
}
/**
 * \fn thumbSlider_valueEmitted
 * \brief Slot to handle signals from the thumb slider.
 */
void MainWindow::thumbSlider_valueEmitted(int value)
{
    if (!avifileinfo)
    {
        if (value)
            thumbSlider->reset();
        return;
    }

    if (playing)
    {
        if (admPreview::getPreviewMode() != ADM_PREVIEW_NONE)
            return;
        sendAction(ACT_PlayAvi); // stop playback;
        dragWhilePlay = true;
        return;
    }
    else if (dragWhilePlay && !value)
    {
        dragWhilePlay = false;
        if (admPreview::getPreviewMode() == ADM_PREVIEW_NONE)
            sendAction(ACT_PlayAvi); // resume playback;
        return;
    }

    if (!value)
        return;

    bool success = true;
    if (value > 0)
        success = admPreview::nextKeyFrame();
    else
        success = admPreview::previousKeyFrame();
    if (success)
    {
        uint64_t total = video_body->getVideoDuration();
        if (total)
        {
            uint64_t pts = admPreview::getCurrentPts();
            UI_setCurrentTime(pts);

            double percentage = pts;
            percentage /= total;
            percentage *= 100;

            UI_setScale(percentage);
        }
        return;
    }
    thumbSlider->reset();
    dragWhilePlay = false;
}

void MainWindow::volumeChange(int u)
{
    if (_upd_in_progres || !ui.toolButtonAudioToggle->isChecked())
        return;

    _upd_in_progres++;

    int vol = ui.horizontalSlider_2->value();

    AVDM_setVolume(vol);
    _upd_in_progres--;
}

void MainWindow::audioToggled(bool checked)
{
    if (checked)
        AVDM_setVolume(ui.horizontalSlider_2->value());
    else
        AVDM_setVolume(0);
}

void MainWindow::previewModeChangedFromMenu(bool flop)
{
    QAction *previewFiltered = findActionInToolBar(ui.toolBar, ACT_PreviewChanged);
    if (previewFiltered)
        previewFiltered->setChecked(flop);
    sendAction(ACT_PreviewChanged);
}

void MainWindow::previewModeChangedFromToolbar(bool flop)
{
    QAction *previewFiltered = findAction(&myMenuVideo, ACT_PreviewChanged);
    if (previewFiltered)
        previewFiltered->setChecked(flop);
    sendAction(ACT_PreviewChanged);
}

void MainWindow::timeChangeFinished(void)
{
    this->setFocus(Qt::OtherFocusReason);
}

void MainWindow::currentTimeChanged(void)
{
    sendAction(ACT_GotoTime);

    this->setFocus(Qt::OtherFocusReason);
}

/**
    \fn currentTimeToClipboard
*/
void MainWindow::currentTimeToClipboard(void)
{
    QString ct = QString();
    ct = ui.currentTime->text();
    QClipboard *clipboard = QApplication::clipboard();
    clipboard->clear();
    clipboard->setText(ct);
}

/**
    \fn setRefreshCap
*/
void MainWindow::setRefreshCap(void)
{
    refreshCapEnabled = false;
    refreshCapValue = 0;
    prefs->get(FEATURES_CAP_REFRESH_ENABLED, &refreshCapEnabled);
    prefs->get(FEATURES_CAP_REFRESH_VALUE, &refreshCapValue);
}

/**
    \fn busyTimerTimeout
*/
void MainWindow::busyTimerTimeout(void)
{
    if ((busyCntr == 0) && QApplication::overrideCursor())
        QApplication::restoreOverrideCursor();
}

/**
    \fn actionSlot
*/
void MainWindow::actionSlot(Action a)
{
    if (a == ACT_SAVE_VIDEO && ui.checkBoxAutoSaveOutput->isChecked())
    {
        QString dir = ui.lineEditAutoSaveOutputDir->text().trimmed();
        if (dir.isEmpty() || !QDir(dir).exists())
        {
            GUI_Error_HIG(admUiText("Save", "保存", "儲存").toUtf8().constData(),
                          admUiText("Auto save folder does not exist.", "自动保存目录不存在。", "自動儲存資料夾不存在。").toUtf8().constData());
            return;
        }
        if (!video_body || !video_body->getNbSegment())
        {
            GUI_Error_HIG(QT_TRANSLATE_NOOP("adm", "No"), QT_TRANSLATE_NOOP("adm", "No file loaded"));
            return;
        }
        int muxerIndex = UI_GetCurrentFormat();
        const char *defaultExtension = ADM_MuxerGetDefaultExtension(muxerIndex);
        char target[4096];
        if (!UI_buildSuggestedSavePath(defaultExtension, dir.toUtf8().constData(), target, sizeof(target)))
        {
            GUI_Error_HIG(admUiText("Save", "保存", "儲存").toUtf8().constData(),
                          admUiText("Cannot build output file name.", "无法生成输出文件名。", "無法產生輸出檔名。").toUtf8().constData());
            return;
        }
        A_SaveWrapper(target);
        return;
    }

    if (a == ACT_EXIT || a == ACT_CLOSE || (a == ACT_PlayAvi && !playing) ||
        (a > ACT_NAVIGATE_BEGIN && a < ACT_NAVIGATE_END))
        thumbSlider->reset();
    if (a == ACT_PlayAvi && avifileinfo) // ugly
    {
        playing = !playing;
        setMenuItemsEnabledState();
        playing = !playing;
        if (busyCntr && QApplication::overrideCursor())
            QApplication::restoreOverrideCursor();
    }
    if (a > ACT_STAGED_BEGIN && a < ACT_STAGED_END)
    {
        actionLock++;
        HandleAction(a);
        actionLock--;
        if (!stagedActionSuccess)
            return; // currently, no action requires enabling or disabling menu items
        switch (a)
        {
        case ACT_SetHDRConfig:
            a = ACT_Refresh;
            break;
#ifdef USE_LIBPOSTPROC
        case ACT_SetPostProcessing:
            a = ACT_Refresh;
            break;
#endif
        case ACT_SelectTime:
            a = ACT_GotoTime;
            break;
        default:
            break;
        }
    }
    if (a > ACT_NAVIGATE_BEGIN && a < ACT_NAVIGATE_END)
    {
        busyTimer.stop();
        busyCntr++;
        if (!QApplication::overrideCursor())
            QApplication::setOverrideCursor(Qt::BusyCursor);
    }
    actionLock++;
    HandleAction(a);
    actionLock--;
    setMenuItemsEnabledState();
    if (a > ACT_NAVIGATE_BEGIN && a < ACT_NAVIGATE_END)
    {
        busyCntr--;
        if (busyCntr == 0)
            busyTimer.start(100);
    }
}

/**
    \fn sendAction
*/
void MainWindow::sendAction(Action a)
{
    if (((a >= ACT_Back1Second) && (a <= ACT_Forward1Mn) || (a == ACT_GotoMarkA) || (a == ACT_GotoMarkB)) &&
        (playing || (navigateWhilePlayingState != 0)))
        navigateWhilePlaying(a);
    else if (((a == ACT_PreviousFrame) || (a == ACT_PreviousKFrame)) && (playing || (navigateWhilePlayingState != 0)))
        navigateWhilePlaying(ACT_SeekBackward);
    else if (((a == ACT_NextFrame) || (a == ACT_NextKFrame)) && (playing || (navigateWhilePlayingState != 0)))
        navigateWhilePlaying(ACT_SeekForward);
    else if (a > ACT_NAVIGATE_BEGIN && a < ACT_NAVIGATE_END && a != ACT_Scale)
    {
        if (actionLock <= NAVIGATION_ACTION_LOCK_THRESHOLD)
            emit actionSignal(a);
    }
    else
    {
        // printf("Sending internal event %d\n",(int)a);
        emit actionSignal(a);
    }
}

/**
    \fn navigateWhilePlaying
*/
void MainWindow::navigateWhilePlaying(Action a)
{
    if (navigateWhilePlayingState != 0)
    {
        navigateWhilePlayingPendingAction = a;
        return;
    }
    emit actionSignal(ACT_PlayAvi);
    printf("navigateWhilePlaying\n");
    navigateWhilePlayingState = 1;
    navigateWhilePlayingAction = a;
    navigateWhilePlayingPendingAction = ACT_INVALID;
    navigateWhilePlayingTimer.stop();
    navigateWhilePlayingTimer.start(10);
}

/**
 * \fn navigateWhilePlayingTimerTimeout
 */
void MainWindow::navigateWhilePlayingTimerTimeout(void)
{
    switch (navigateWhilePlayingState)
    {
    case 1:
        if (!playing) // wait for stop
        {
            navigateWhilePlayingState++;
            emit actionSignal(navigateWhilePlayingAction);
        }
        break;
    case 2:
        if (actionLock == 0) // wait for action to completed
        {
            if (navigateWhilePlayingPendingAction != ACT_INVALID)
            {
                navigateWhilePlayingAction = navigateWhilePlayingPendingAction;
                navigateWhilePlayingState = 1;
                navigateWhilePlayingPendingAction = ACT_INVALID;
                break;
            }
            navigateWhilePlayingState = 0;
            navigateWhilePlayingTimer.stop();
            emit actionSignal(ACT_PlayAvi);
        }
        break;
    default:
        navigateWhilePlayingState = 0;
        navigateWhilePlayingTimer.stop();
        break;
    }
}

/**
    \fn setTimeDisplaySize
    \brief Enlarge current time display for "windows11" Qt style
*/
void MainWindow::setTimeDisplaySize(void)
{
    // set the size of the current time display to fit the content
    QString text = "00:00:00.000"; // Don't translate this.
    QString oldtext = ui.currentTime->text();
    ui.currentTime->setText(text); // Override ui translations to make sure we use point as decimal separator.
    QRect ctrect = ui.currentTime->fontMetrics().boundingRect(text);
#define DEFAULT_CT_DISPLAY_STRETCH_FACTOR 1.15
#ifdef _WIN32
    QStyle *currentStyle = QApplication::style();
    QString currentStyleName = "unknown";
    if (currentStyle)
        currentStyleName = currentStyle->objectName().toLower(); // style names are case-insensitive

    float stretchFactor =
        (0 == strcmp(currentStyleName.toUtf8().constData(), "windows11")) ? 1.25 : DEFAULT_CT_DISPLAY_STRETCH_FACTOR;
    ui.currentTime->setFixedSize(stretchFactor * ctrect.width(), ui.currentTime->height());
#else
    ui.currentTime->setFixedSize(DEFAULT_CT_DISPLAY_STRETCH_FACTOR * ctrect.width(), ui.currentTime->height());
#endif
#undef DEFAULT_CT_DISPLAY_STRETCH_FACTOR
    ui.currentTime->setText(oldtext);
}

/**
    \fn ctor
*/
MainWindow::MainWindow(const vector<IScriptEngine *> &scriptEngines) : _scriptEngines(scriptEngines), QMainWindow()
{
    MainWindow::mainWindowSingleton = this;
    qtRegisterDialog(this);
    ui.setupUi(this);
    admApplyCustomUiTranslations(ui);
    if (QSettings *qset = qtSettingsCreate())
    {
        bool autoSaveEnabled = qset->value("autoSaveOutput/enabled", false).toBool();
        QString autoSaveDir = qset->value("autoSaveOutput/dir").toString();
        ui.checkBoxAutoSaveOutput->setChecked(autoSaveEnabled);
        ui.lineEditAutoSaveOutputDir->setText(autoSaveDir);
        ui.lineEditAutoSaveOutputDir->setEnabled(autoSaveEnabled);
        ui.pushButtonAutoSaveOutputBrowse->setEnabled(autoSaveEnabled);
        delete qset;
    }
    dragState = dragState_Normal;
    navigateByTimeButtonsState = 0;
    navigateWhilePlayingState = 0;
    recentFiles = NULL;
    recentProjects = NULL;
    actionLock = 0;
    busyCntr = 0;
    busyTimer.setSingleShot(true);
    statusBarWidget = NULL;
    statusBarInfo = NULL;
    statusBarMessage = NULL;
    statusBarTimer.setSingleShot(true);
    statusBarTimer.stop();
    statusBarFlashTimer.setSingleShot(true);
    statusBarFlashTimer.stop();

    connect(ui.actionViewStatusBar, SIGNAL(toggled(bool)), this, SLOT(setStatusBarEnabled(bool)));
    connect(&statusBarTimer, SIGNAL(timeout()), this, SLOT(statusBarTimerTimeout()));

#if defined(__APPLE__) && defined(USE_SDL)
    // ui.actionAbout_avidemux->setMenuRole(QAction::NoRole);
    // ui.actionPreferences->setMenuRole(QAction::NoRole);
    // ui.actionQuit->setMenuRole(QAction::NoRole);
#endif

#ifdef __APPLE__
    ui.navButtonsLayout->setSpacing(2);
    // Qt upscales 2x sized icons in the toolbar, making them huge and pixelated in HiDPI conditions, WTF?
    ui.toolBar->setIconSize(QSize(24, 24));
#endif
    //
    connect(this, SIGNAL(actionSignal(Action)), this, SLOT(actionSlot(Action)));
    //
    connect(this, SIGNAL(updateAvailable(int, std::string, std::string)), this,
            SLOT(updateAvailableSlot(int, std::string, std::string)));

    /*
    Connect our button to buttonPressed
    */
#define PROCESS CONNECT_TB
    LIST_OF_BUTTONS
#undef PROCESS

    // ACT_VideoCodecChanged
    connect(ui.comboBoxProfile, SIGNAL(activated(int)), this, SLOT(profileChanged(int)));
    connect(ui.pushButtonProfileSave, SIGNAL(clicked()), this, SLOT(saveProfilePressed()));
    connect(ui.comboBoxProfileResizeMode, SIGNAL(activated(int)), this, SLOT(profileResizeChanged(int)));
    connect(ui.spinBoxProfileWidth, SIGNAL(valueChanged(int)), this, SLOT(profileResizeValueChanged(int)));
    connect(ui.spinBoxProfileHeight, SIGNAL(valueChanged(int)), this, SLOT(profileResizeValueChanged(int)));
    connect(ui.checkBoxAutoSaveOutput, SIGNAL(toggled(bool)), this, SLOT(autoSaveOutputToggled(bool)));
    connect(ui.pushButtonAutoSaveOutputBrowse, SIGNAL(clicked()), this, SLOT(autoSaveOutputBrowsePressed()));
    connect(ui.lineEditAutoSaveOutputDir, SIGNAL(textChanged(QString)), this, SLOT(autoSaveOutputDirChanged(QString)));
    connect(ui.comboBoxVideo, SIGNAL(activated(int)), this, SLOT(comboChanged(int)));
    connect(ui.comboBoxAudio, SIGNAL(activated(int)), this, SLOT(comboChanged(int)));

    // Slider
    slider = ui.horizontalSlider;
    ADM_mwNavSlider *qslider = (ADM_mwNavSlider *)slider;
    slider->setMinimum(0);
    slider->setMaximum(ADM_LARGE_SCALE);
#if !(defined(__APPLE__) && QT_VERSION >= QT_VERSION_CHECK(5, 10, 0))
    slider->setTickInterval(ADM_SCALE_INCREMENT);
    slider->setTickPosition(QSlider::TicksBothSides);
#endif
    connect(slider, SIGNAL(valueChanged(int)), this, SLOT(sliderValueChanged(int)));
    connect(slider, SIGNAL(sliderMoved(int)), this, SLOT(sliderMoved(int)));
    connect(slider, SIGNAL(sliderReleased()), this, SLOT(sliderReleased()));
    connect(slider, SIGNAL(sliderPressed()), this, SLOT(sliderPressed()));
    connect(qslider, SIGNAL(sliderAction(int)), this, SLOT(sliderWheel(int)));

    connect(&dragTimer, SIGNAL(timeout()), this, SLOT(dragTimerTimeout()));
    connect(&busyTimer, SIGNAL(timeout()), this, SLOT(busyTimerTimeout()));
    connect(&navigateWhilePlayingTimer, SIGNAL(timeout()), this, SLOT(navigateWhilePlayingTimerTimeout()));
    // Navigation
    ui.toolButtonBackOneMinute->installEventFilter(this);
    ui.toolButtonForwardOneMinute->installEventFilter(this);

    // Thumb slider
    ui.sliderPlaceHolder->installEventFilter(this);
    thumbSlider = new ThumbSlider(ui.sliderPlaceHolder);
    connect(thumbSlider, SIGNAL(valueEmitted(int)), this, SLOT(thumbSlider_valueEmitted(int)));

    // Volume slider
    QSlider *volSlider = ui.horizontalSlider_2;
    volSlider->setMinimum(0);
    volSlider->setMaximum(100);
    connect(volSlider, SIGNAL(valueChanged(int)), this, SLOT(volumeChange(int)));
    connect(ui.toolButtonAudioToggle, SIGNAL(clicked(bool)), this, SLOT(audioToggled(bool)));

    // default state
    bool b = 0;
    ui.pushButtonVideoConf->setEnabled(b);
    ui.pushButtonVideoFilter->setEnabled(b);
    ui.pushButtonAudioConf->setEnabled(b);
    ui.pushButtonAudioFilter->setEnabled(b);

    /* Time Shift */
    connect(ui.checkBox_TimeShift, SIGNAL(stateChanged(int)), this, SLOT(checkChanged(int)));
    connect(ui.spinBox_TimeValue, SIGNAL(valueChanged(int)), this, SLOT(timeChanged(int)));
    connect(ui.spinBox_TimeValue, SIGNAL(editingFinished()), this, SLOT(timeChangeFinished()));
#if 0 /* it is read-only */
    QRegExp timeRegExp("^[0-9]{2}:[0-5][0-9]:[0-5][0-9]\\.[0-9]{3}$");
    QRegExpValidator *timeValidator = new QRegExpValidator(timeRegExp, this);
    ui.currentTime->setValidator(timeValidator);
    ui.currentTime->setInputMask("99:99:99.999");
#endif
    // set the size of the current time display to fit the content
    QString text = "00:00:00.000"; // Don't translate this.
#ifdef USE_CUSTOM_TIME_DISPLAY_FONT
    ui.currentTime->setFont(QFont("ADM7SEG"));
#endif
    ui.currentTime->setText(text); // Override ui translations to make sure we use point as decimal separator.
    setTimeDisplaySize();

    text = QString("/ ") + text;
    ui.totalTime->setText(text); // Override ui translations here too.

    // connect(ui.currentTime, SIGNAL(editingFinished()), this, SLOT(currentTimeChanged()));

    // Build file,... menu
    addScriptEnginesToFileMenu(myMenuFile);
    addScriptShellsToToolsMenu(myMenuTool);

    QString rFiles = QString::fromUtf8(QT_TRANSLATE_NOOP("qgui2", "Recent Files"));
    QString rProjects = QString::fromUtf8(QT_TRANSLATE_NOOP("qgui2", "Recent Projects"));

    recentFiles = new QMenu(rFiles, ui.menuRecent);
    recentProjects = new QMenu(rProjects, ui.menuRecent);
    ui.menuRecent->addMenu(recentFiles);
    ui.menuRecent->addMenu(recentProjects);
    connect(this->recentFiles, SIGNAL(triggered(QAction *)), this, SLOT(searchRecentFiles(QAction *)));
    connect(this->recentProjects, SIGNAL(triggered(QAction *)), this, SLOT(searchRecentProjects(QAction *)));

    addSessionRestoreToRecentMenu(myMenuRecent);

    buildMyMenu();
    buildCustomMenu(); // action lists are populated (i.e. buildActionLists() called) within buildCustomMenu()
    buildButtonLists();

#define AUTOREPEAT_TOOLBUTTON(x)                                                                                       \
    ui.x->setAutoRepeat(true);                                                                                         \
    ui.x->setAutoRepeatDelay(500);                                                                                     \
    ui.x->setAutoRepeatInterval(100);
    AUTOREPEAT_TOOLBUTTON(toolButtonPreviousFrame)
    AUTOREPEAT_TOOLBUTTON(toolButtonNextFrame)
    AUTOREPEAT_TOOLBUTTON(toolButtonPreviousIntraFrame)
    AUTOREPEAT_TOOLBUTTON(toolButtonNextIntraFrame)

    actionHDRSeparator = ui.toolBar->insertSeparator(ui.actionHDRSettings);

    // Crash in some cases addScriptReferencesToHelpMenu();
    QAction *previewFiltered = findAction(&myMenuVideo, ACT_PreviewChanged);
    if (previewFiltered)
        connect(previewFiltered, SIGNAL(toggled(bool)), this, SLOT(previewModeChangedFromMenu(bool)));
    previewFiltered = findActionInToolBar(ui.toolBar, ACT_PreviewChanged);
    if (previewFiltered)
        connect(previewFiltered, SIGNAL(toggled(bool)), this, SLOT(previewModeChangedFromToolbar(bool)));

    // Add action to show all dock widgets and move the toolbar to its default area
    QAction *restoreDefaults = new QAction(QT_TRANSLATE_NOOP("qgui2", "Restore defaults"), this);
    ui.menuToolbars->addSeparator();
    ui.menuToolbars->addAction(restoreDefaults);

    connect(ui.menuToolbars->actions().last(), SIGNAL(triggered(bool)), this, SLOT(restoreDefaultWidgetState(bool)));

    QStyle *currentStyle = QApplication::style();
    defaultStyle = currentStyle->objectName().toLower(); // style names are case-insensitive
    if (NULL != getenv("ADM_QT_STYLE_VERBOSE"))
    {
        QStringList listOfStyles = QStyleFactory::keys();
        ADM_info("Built-in Qt styles:\n");
        for (auto it = listOfStyles.begin(); it < listOfStyles.end(); it++)
        {
            printf("\t%s\n", (*it).toUtf8().constData());
        }
        ADM_info("Default Qt style: %s\n", defaultStyle.toUtf8().constData());
    }
    defaultThemeAction = new QAction(QT_TRANSLATE_NOOP("qgui2", "Default theme"), this);
    defaultThemeAction->setCheckable(true);
    ui.menuThemes->addAction(defaultThemeAction);
    defaultThemeAction->setChecked(true);
    connect(defaultThemeAction, SIGNAL(triggered(bool)), this, SLOT(setDefaultThemeSlot(bool)));

    lightThemeAction = new QAction(QT_TRANSLATE_NOOP("qgui2", "Light theme"), this);
    lightThemeAction->setCheckable(true);
    ui.menuThemes->addAction(lightThemeAction);
    connect(lightThemeAction, SIGNAL(triggered(bool)), this, SLOT(setLightThemeSlot(bool)));

    darkThemeAction = new QAction(QT_TRANSLATE_NOOP("qgui2", "Dark theme"), this);
    darkThemeAction->setCheckable(true);
    ui.menuThemes->addAction(darkThemeAction);
    connect(darkThemeAction, SIGNAL(triggered(bool)), this, SLOT(setDarkThemeSlot(bool)));

    this->installEventFilter(this);
    slider->installEventFilter(this);

    // ui.currentTime->installEventFilter(this);

    this->setFocus(Qt::OtherFocusReason);

    setAcceptDrops(true);

    // clang-format off
#ifndef __APPLE__
    setWindowIcon(QIcon(MKICON(avidemux-icon)));
#else
    setWindowIcon(QIcon(MKOSXICON(avidemux)));
#endif
    // clang-format on

    // Hook also the toolbar
    connect(ui.toolBar, SIGNAL(actionTriggered(QAction *)), this, SLOT(searchToolBar(QAction *)));
    connect(ui.toolBar, SIGNAL(orientationChanged(Qt::Orientation)), this,
            SLOT(toolbarOrientationChangedSlot(Qt::Orientation)));
    // connect(ui.toolBar_2,SIGNAL(actionTriggered ( QAction *)),this,SLOT(searchToolBar(QAction *)));

    QWidget *dummy0 = new QWidget();
    QWidget *dummy1 = new QWidget();
    QWidget *dummy2 = new QWidget();
    QWidget *dummy3 = new QWidget();
    QWidget *dummy4 = new QWidget();

    ui.codecWidget->setTitleBarWidget(dummy0);
    ui.navigationWidget->setTitleBarWidget(dummy1);
    ui.selectionWidget->setTitleBarWidget(dummy2);
    ui.volumeWidget->setTitleBarWidget(dummy3);
    ui.audioMetreWidget->setTitleBarWidget(dummy4);

    widgetsUpdateTooltips();

    this->adjustSize();
    ui.currentTime->setTextMargins(0, 0, 0, 0); // some Qt themes mess with text margins

    threshold = RESIZE_THRESHOLD;
    actZoomCalled = false;
    ignoreResizeEvent = false;
    blockResizing = false;
    blockZoomChanges = true;
    dragWhilePlay = false;
    statusBarInfo_Zoom = 100;

    QuiTaskBarProgress = createADMTaskBarProgress();
}
/**
    \fn searchToolBar
*/
typedef struct
{
    const char *name;
    Action event;
} toolBarTranslate;

static toolBarTranslate toolbar[] = {{"actionOpen", ACT_OPEN_VIDEO},
                                     {"actionSave_video", ACT_SAVE_VIDEO},
                                     {"actionProperties", ACT_VIDEO_PROPERTIES},
                                     {"actionLoad_run_project", ACT_RUN_SCRIPT},
                                     {"actionSave_project", ACT_SAVE_PY_SCRIPT},
                                     {"actionPlayFiltered", ACT_PreviewChanged},
                                     {"actionHDRSettings", ACT_SetHDRConfig},

                                     {NULL, ACT_DUMMY}};
void MainWindow::searchToolBar(QAction *action)
{
    toolBarTranslate *t = toolbar;

    char *name = ADM_strdup(action->objectName().toUtf8().constData());
    while (t->name)
    {
        if (!strcmp(name, t->name))
        {
            sendAction(t->event);
            ADM_dealloc(name);
            return;
        }
        t++;
    }
    ADM_warning("Toolbar:Cannot handle %s\n", name);
    ADM_dealloc(name);
}

/**
    \fn findActionInToolBar
*/
QAction *findActionInToolBar(QToolBar *tb, Action action)
{
    toolBarTranslate *t = toolbar;
    const char *name = NULL;
    while (t->name)
    {
        if (t->event != action)
        {
            t++;
            continue;
        }
        name = t->name;
        break;
    }
    if (!name)
        return NULL;

    QAction *a = NULL;
    for (int i = 0; i < tb->actions().size(); i++)
    {
        a = tb->actions().at(i);
        QString s = a->objectName();
        if (s.isEmpty())
            continue;
        if (!strcmp(s.toUtf8().constData(), name))
            return a;
    }
    return NULL;
}

/**
    \fn getMenuEntryForAction
*/
const MenuEntry *getMenuEntryForAction(std::vector<MenuEntry> *list, QAction *action)
{
    for (int i = 0; i < list->size(); i++)
    {
        MenuEntry *candidate = &list->at(i);
        if (candidate->cookie == (void *)action)
            return candidate;
    }
    return NULL;
}

/**
    \fn findAction
*/
static QAction *findAction(std::vector<MenuEntry> *list, Action action)
{
    for (int i = 0; i < list->size(); i++)
    {
        MenuEntry *m = &list->at(i);
        if (m->type != MENU_ACTION && m->type != MENU_SUBACTION)
            continue;
        if (m->event != action)
            continue;
        QAction *a = (QAction *)m->cookie;
        return a;
    }
    return NULL;
}

/**
    \fn buildFileMenu
*/
bool MainWindow::buildMenu(QMenu *root, MenuEntry *menu, int nb)
{
    bool alt = false, swpud = false;
    if (menu == &myMenuEdit[0] || menu == &myMenuGo[0])
    {
        prefs->get(KEYBOARD_SHORTCUTS_USE_ALTERNATE_KBD_SHORTCUTS, &alt);
        prefs->get(KEYBOARD_SHORTCUTS_SWAP_UP_DOWN_KEYS, &swpud);
    }
    QMenu *subMenu = NULL;
    for (int i = 0; i < nb; i++)
    {
        MenuEntry *m = menu + i;
        QString qs;
        if (m->translated)
            qs = QString::fromUtf8(m->text.c_str());
        else
            qs = QString::fromUtf8(QT_TRANSLATE_NOOP("adm", m->text.c_str()));
        switch (m->type)
        {
        case MENU_SEPARATOR:
            root->addSeparator();
            break;
        case MENU_SUBMENU: {
            subMenu = root->addMenu(qs);
#ifdef BROKEN_PALETTE_PROPAGATION
            subMenus.push_back(subMenu);
#endif
        }
        break;
        case MENU_SUBACTION:
        case MENU_ACTION: {
            QMenu *insert = root;
            if (m->type == MENU_SUBACTION)
                insert = subMenu;
            QAction *a = NULL;
            if (m->icon)
            {
                QIcon icon(m->icon);
                a = insert->addAction(icon, qs);
            }
            else
                a = insert->addAction(qs);
            ADM_assert(a);
            a->setObjectName(m->text.c_str());
#if defined(__APPLE__)
            switch (m->event)
            {
            case (ACT_EXIT):
                a->setMenuRole(QAction::QuitRole);
                break;
            case (ACT_PREFERENCES):
                a->setMenuRole(QAction::PreferencesRole);
                break;
            case (ACT_ABOUT):
                a->setMenuRole(QAction::AboutRole);
                break;
            default:
                a->setMenuRole(QAction::NoRole);
            }
#endif
            m->cookie = (void *)a;
            if (m->shortCut)
            {
                if (swpud && !strcmp(m->shortCut, "Up"))
                    a->setShortcut(Qt::Key_Down);
                else if (swpud && !strcmp(m->shortCut, "Down"))
                    a->setShortcut(Qt::Key_Up);

                if (alt)
                {
                    std::string sc = "";
                    switch (m->event)
                    {
                    case ACT_MarkA:
                        prefs->get(KEYBOARD_SHORTCUTS_ALT_MARK_A, sc);
                        break;
                    case ACT_MarkB:
                        prefs->get(KEYBOARD_SHORTCUTS_ALT_MARK_B, sc);
                        break;
                    case ACT_ResetMarkerA:
                        prefs->get(KEYBOARD_SHORTCUTS_ALT_RESET_MARK_A, sc);
                        break;
                    case ACT_ResetMarkerB:
                        prefs->get(KEYBOARD_SHORTCUTS_ALT_RESET_MARK_B, sc);
                        break;
                    case ACT_ResetMarkers:
                        prefs->get(KEYBOARD_SHORTCUTS_ALT_RESET_MARKERS, sc);
                        break;
                    case ACT_GotoMarkA:
                        prefs->get(KEYBOARD_SHORTCUTS_ALT_GOTO_MARK_A, sc);
                        break;
                    case ACT_GotoMarkB:
                        prefs->get(KEYBOARD_SHORTCUTS_ALT_GOTO_MARK_B, sc);
                        break;
                    case ACT_Begin:
                        prefs->get(KEYBOARD_SHORTCUTS_ALT_BEGIN, sc);
                        break;
                    case ACT_End:
                        prefs->get(KEYBOARD_SHORTCUTS_ALT_END, sc);
                        break;
                    case ACT_Delete:
                        prefs->get(KEYBOARD_SHORTCUTS_ALT_DELETE, sc);
                        break;
                    default:
                        sc = std::string(m->shortCut);
                    }
                    QString qsc = QString::fromUtf8(sc.c_str());
                    a->setShortcut(QKeySequence(qsc));
                    break;
                }
                QKeySequence s(m->shortCut);
                a->setShortcut(s);
            }
            break;
        }
        default:
            break;
        }
    }
    return true;
}

/**
    buildFileMenu
*/
bool MainWindow::buildMyMenu(void)
{
    connect(ui.menuFile, SIGNAL(triggered(QAction *)), this, SLOT(searchFileMenu(QAction *)));
    buildMenu(ui.menuFile, &myMenuFile[0], myMenuFile.size());

    connect(ui.menuRecent, SIGNAL(triggered(QAction *)), this, SLOT(searchRecentMenu(QAction *)));
    buildMenu(ui.menuRecent, &myMenuRecent[0], myMenuRecent.size());

    connect(ui.menuEdit, SIGNAL(triggered(QAction *)), this, SLOT(searchEditMenu(QAction *)));
    buildMenu(ui.menuEdit, &myMenuEdit[0], myMenuEdit.size());

    connect(ui.menuVideo, SIGNAL(triggered(QAction *)), this, SLOT(searchVideoMenu(QAction *)));
    buildMenu(ui.menuVideo, &myMenuVideo[0], myMenuVideo.size());

    connect(ui.menuAudio, SIGNAL(triggered(QAction *)), this, SLOT(searchAudioMenu(QAction *)));
    buildMenu(ui.menuAudio, &myMenuAudio[0], myMenuAudio.size());

    connect(ui.menuHelp, SIGNAL(triggered(QAction *)), this, SLOT(searchHelpMenu(QAction *)));
    buildMenu(ui.menuHelp, &myMenuHelp[0], myMenuHelp.size());

    connect(ui.menuTools, SIGNAL(triggered(QAction *)), this, SLOT(searchToolMenu(QAction *)));

    if (myMenuTool.size() > 0)
    {
        buildMenu(ui.menuTools, &myMenuTool[0], myMenuTool.size());
    }

    connect(ui.menuGo, SIGNAL(triggered(QAction *)), this, SLOT(searchGoMenu(QAction *)));
    buildMenu(ui.menuGo, &myMenuGo[0], myMenuGo.size());

    connect(ui.menuView, SIGNAL(triggered(QAction *)), this, SLOT(searchViewMenu(QAction *)));
    buildMenu(ui.menuView, &myMenuView[0], myMenuView.size());

    QAction *a = findAction(&myMenuVideo, ACT_PreviewChanged);
    if (a)
        a->setCheckable(true);

    return true;
}

/**
    \fn buildActionLists
*/
void MainWindow::buildActionLists(void)
{
    ActionsAvailableWhenFileLoaded.clear();
    ActionsDisabledOnPlayback.clear();
    ActionsAlwaysAvailable.clear();

    // Make a list of the items that are enabled/disabled depending if video is loaded or  not
    //-----------------------------------------------------------------------------------
#define PUSH_LOADED(x, act)                                                                                            \
    {                                                                                                                  \
        QAction *a = findAction(&myMenu##x, act);                                                                      \
        if (a)                                                                                                         \
            ActionsAvailableWhenFileLoaded.push_back(a);                                                               \
    }
    PUSH_LOADED(File, ACT_APPEND_VIDEO)
    PUSH_LOADED(File, ACT_SAVE_VIDEO)
    PUSH_LOADED(File, ACT_SAVE_QUEUE)

    PUSH_LOADED(File, ACT_SAVE_BMP)
    PUSH_LOADED(File, ACT_SAVE_PNG)
    PUSH_LOADED(File, ACT_SAVE_JPG)
    PUSH_LOADED(File, ACT_SAVE_BUNCH_OF_JPG)

    for (uint32_t engineIdx = 0; engineIdx < _scriptEngines.size(); engineIdx++)
        PUSH_LOADED(File, (Action)(ACT_SCRIPT_ENGINE_FIRST + (engineIdx * 3) + 2))

    PUSH_LOADED(File, ACT_CLOSE)
    PUSH_LOADED(File, ACT_VIDEO_PROPERTIES)

    PUSH_LOADED(Edit, ACT_Copy)

    PUSH_LOADED(Edit, ACT_MarkA)
    PUSH_LOADED(Edit, ACT_MarkB)

    PUSH_LOADED(View, ACT_ZOOM_1_4)
    PUSH_LOADED(View, ACT_ZOOM_1_2)
    PUSH_LOADED(View, ACT_ZOOM_1_1)
    PUSH_LOADED(View, ACT_ZOOM_2_1)
    PUSH_LOADED(View, ACT_ZOOM_FIT_IN)

#define PUSH_LOADED_TOOLBAR(action)                                                                                    \
    {                                                                                                                  \
        QAction *a = findActionInToolBar(ui.toolBar, action);                                                          \
        if (a)                                                                                                         \
            ActionsAvailableWhenFileLoaded.push_back(a);                                                               \
    }
    if (ADM_mx_getNbMuxers()) // Don't enable "Save" if we've got zero muxers.
        PUSH_LOADED_TOOLBAR(ACT_SAVE_VIDEO)
    PUSH_LOADED_TOOLBAR(ACT_VIDEO_PROPERTIES)
    PUSH_LOADED_TOOLBAR(ACT_PreviewChanged)

#define PUSH_FULL_MENU_LOADED(x, tailOffset)                                                                           \
    for (int i = 0; i < ui.x->actions().size() - tailOffset; i++)                                                      \
    {                                                                                                                  \
        QAction *a = ui.x->actions().at(i);                                                                            \
        if (a->objectName().isEmpty())                                                                                 \
            continue;                                                                                                  \
        ActionsAvailableWhenFileLoaded.push_back(a);                                                                   \
    }
#define PUSH_FULL_MENU_PLAYBACK(x, tailOffset)                                                                         \
    for (int i = 0; i < ui.x->actions().size() - tailOffset; i++)                                                      \
    {                                                                                                                  \
        QAction *a = ui.x->actions().at(i);                                                                            \
        if (a->objectName().isEmpty())                                                                                 \
            continue;                                                                                                  \
        ActionsDisabledOnPlayback.push_back(a);                                                                        \
    }

    PUSH_FULL_MENU_LOADED(menuAudio, 2)
    PUSH_FULL_MENU_LOADED(menuAuto, 0)
    PUSH_FULL_MENU_LOADED(menuGo, 0)

    // Item disabled on playback
    for (int i = 2; i < ui.menuView->actions().size(); i++)
    { // allow hiding widgets during playback
        ActionsDisabledOnPlayback.push_back(ui.menuView->actions().at(i));
    }

    for (int i = 1; i < ui.menuGo->actions().size(); i++)
    { // let "Play/Stop" stay enabled during playback
        if (myMenuGo.at(i).event == ACT_GotoMarkA)
            continue;
        if (myMenuGo.at(i).event == ACT_GotoMarkB)
            continue;
        ActionsDisabledOnPlayback.push_back(ui.menuGo->actions().at(i));
        if (myMenuGo.at(i).event == ACT_SelectTime)
            break;
    }

    PUSH_FULL_MENU_PLAYBACK(menuFile, 1)
    PUSH_FULL_MENU_PLAYBACK(menuEdit, 0)
    PUSH_FULL_MENU_PLAYBACK(menuVideo, 0)
    PUSH_FULL_MENU_PLAYBACK(menuAudio, 0)
    PUSH_FULL_MENU_PLAYBACK(menuAuto, 0)
    PUSH_FULL_MENU_PLAYBACK(menuHelp, 0)
    PUSH_FULL_MENU_PLAYBACK(toolBar, 0)

    if (recentFiles)
        for (int i = 0; i < recentFiles->actions().size(); i++)
            ActionsDisabledOnPlayback.push_back(recentFiles->actions().at(i));
    if (recentProjects)
        for (int i = 0; i < recentProjects->actions().size(); i++)
            ActionsDisabledOnPlayback.push_back(recentProjects->actions().at(i));

    ActionsDisabledOnPlayback.push_back(ui.menuRecent->actions().back());

    // "Always available" below doesn't override the list of menu items disabled during playback

#define PUSH_ALWAYS_AVAILABLE(menu, event)                                                                             \
    {                                                                                                                  \
        QAction *a = findAction(&myMenu##menu, event);                                                                 \
        if (a)                                                                                                         \
            ActionsAlwaysAvailable.push_back(a);                                                                       \
    }

    PUSH_ALWAYS_AVAILABLE(File, ACT_OPEN_VIDEO)
    PUSH_ALWAYS_AVAILABLE(File, ACT_AVS_PROXY)
    PUSH_ALWAYS_AVAILABLE(File, ACT_EXIT)

    PUSH_ALWAYS_AVAILABLE(Edit, ACT_PREFERENCES)
    PUSH_ALWAYS_AVAILABLE(Edit, ACT_SaveAsDefault)
    PUSH_ALWAYS_AVAILABLE(Edit, ACT_LoadDefault)

#define PUSH_ALWAYS_AVAILABLE_TOOLBAR(event)                                                                           \
    {                                                                                                                  \
        QAction *a = findActionInToolBar(ui.toolBar, event);                                                           \
        if (a)                                                                                                         \
            ActionsAlwaysAvailable.push_back(a);                                                                       \
    }

    PUSH_ALWAYS_AVAILABLE_TOOLBAR(ACT_OPEN_VIDEO)

#define PUSH_ALWAYS_AVAILABLE_TOOLBAR_SCRIPTS X(0) X(1) X(2) X(3) X(4) X(5) X(6) X(7) X(8) X(9)
#define X(key)                                                                                                         \
    if (ui.actionScript##key->isVisible())                                                                             \
    {                                                                                                                  \
        ActionsAlwaysAvailable.push_back(ui.actionScript##key);                                                        \
    }
    PUSH_ALWAYS_AVAILABLE_TOOLBAR_SCRIPTS
#undef X
#undef PUSH_ALWAYS_AVAILABLE_TOOLBAR_SCRIPTS

#define PUSH_FULL_MENU_ALWAYS_AVAILABLE(menu)                                                                          \
    for (int i = 0; i < ui.menu->actions().size(); i++)                                                                \
        ActionsAlwaysAvailable.push_back(ui.menu->actions().at(i));

    PUSH_FULL_MENU_ALWAYS_AVAILABLE(menuHelp)

    if (recentFiles)
        for (int i = 0; i < recentFiles->actions().size(); i++)
            ActionsAlwaysAvailable.push_back(recentFiles->actions().at(i));
    if (recentProjects)
        for (int i = 0; i < recentProjects->actions().size(); i++)
            ActionsAlwaysAvailable.push_back(recentProjects->actions().at(i));
}

/**
    \fn buildButtonLists
*/
void MainWindow::buildButtonLists(void)
{
    ButtonsAvailableWhenFileLoaded.clear();
    ButtonsDisabledOnPlayback.clear();
    PushButtonsAvailableWhenFileLoaded.clear();
    PushButtonsDisabledOnPlayback.clear();

#define ADD_BUTTON_LOADED(x) ButtonsAvailableWhenFileLoaded.push_back(ui.x);
#define ADD_BUTTON_PLAYBACK(x) ButtonsDisabledOnPlayback.push_back(ui.x);

    ADD_BUTTON_LOADED(toolButtonPlay)
    ADD_BUTTON_LOADED(toolButtonPreviousFrame)
    ADD_BUTTON_LOADED(toolButtonNextFrame)
    ADD_BUTTON_LOADED(toolButtonPreviousIntraFrame)
    ADD_BUTTON_LOADED(toolButtonNextIntraFrame)
    ADD_BUTTON_LOADED(toolButtonSetMarkerA)
    ADD_BUTTON_LOADED(toolButtonDeleteSelection)
    ADD_BUTTON_LOADED(toolButtonSetMarkerB)
    ADD_BUTTON_LOADED(toolButtonPreviousCutPoint)
    ADD_BUTTON_LOADED(toolButtonNextCutPoint)
    ADD_BUTTON_LOADED(toolButtonPreviousBlackFrame)
    ADD_BUTTON_LOADED(toolButtonNextBlackFrame)
    ADD_BUTTON_LOADED(toolButtonFirstFrame)
    ADD_BUTTON_LOADED(toolButtonLastFrame)
    ADD_BUTTON_LOADED(toolButtonBackOneMinute)
    ADD_BUTTON_LOADED(toolButtonForwardOneMinute)

    ADD_BUTTON_PLAYBACK(toolButtonPreviousFrame)
    ADD_BUTTON_PLAYBACK(toolButtonNextFrame)
    ADD_BUTTON_PLAYBACK(toolButtonPreviousIntraFrame)
    ADD_BUTTON_PLAYBACK(toolButtonNextIntraFrame)
    ADD_BUTTON_PLAYBACK(toolButtonSetMarkerA)
    ADD_BUTTON_PLAYBACK(toolButtonDeleteSelection)
    ADD_BUTTON_PLAYBACK(toolButtonSetMarkerB)
    ADD_BUTTON_PLAYBACK(toolButtonPreviousCutPoint)
    ADD_BUTTON_PLAYBACK(toolButtonNextCutPoint)
    ADD_BUTTON_PLAYBACK(toolButtonPreviousBlackFrame)
    ADD_BUTTON_PLAYBACK(toolButtonNextBlackFrame)
    ADD_BUTTON_PLAYBACK(toolButtonFirstFrame)
    ADD_BUTTON_PLAYBACK(toolButtonLastFrame)
    // ADD_BUTTON_PLAYBACK(toolButtonBackOneMinute)
    // ADD_BUTTON_PLAYBACK(toolButtonForwardOneMinute)

#define ADD_PUSHBUTTON_LOADED(x) PushButtonsAvailableWhenFileLoaded.push_back(ui.x);
#define ADD_PUSHBUTTON_PLAYBACK(x) PushButtonsDisabledOnPlayback.push_back(ui.x);

    ADD_PUSHBUTTON_LOADED(pushButtonTime)
    ADD_PUSHBUTTON_LOADED(pushButtonJumpToMarkerA)
    ADD_PUSHBUTTON_LOADED(pushButtonJumpToMarkerB)

    ADD_PUSHBUTTON_PLAYBACK(pushButtonTime)
    ADD_PUSHBUTTON_PLAYBACK(pushButtonJumpToMarkerA)
    ADD_PUSHBUTTON_PLAYBACK(pushButtonJumpToMarkerB)

    ADD_PUSHBUTTON_PLAYBACK(pushButtonVideoConf)
    ADD_PUSHBUTTON_PLAYBACK(pushButtonVideoFilter)
    ADD_PUSHBUTTON_PLAYBACK(pushButtonAudioConf)
    ADD_PUSHBUTTON_PLAYBACK(pushButtonAudioFilter)
    ADD_PUSHBUTTON_PLAYBACK(pushButtonFormatConfigure)
}

/**
    \fn setMenuItemsEnabledState
    \brief disable or enable some of the menu items
*/
void MainWindow::setMenuItemsEnabledState(void)
{
    if (playing || (navigateWhilePlayingState != 0)) // this actually doesn't work as it should
    {
        int n = ActionsDisabledOnPlayback.size();
        for (int i = 0; i < n; i++)
            ActionsDisabledOnPlayback[i]->setEnabled(false);

        int ntb = ButtonsDisabledOnPlayback.size();
        for (int i = 0; i < ntb; i++)
            ButtonsDisabledOnPlayback[i]->setEnabled(false);

        ui.toolButtonPlay->setIcon(QIcon(MKICON(player_stop)));
        ui.menuGo->actions().at(0)->setIcon(QIcon(MKICON(player_stop)));

        int npb = PushButtonsDisabledOnPlayback.size();
        for (int i = 0; i < npb; i++)
            PushButtonsDisabledOnPlayback[i]->setEnabled(false);

        ui.checkBox_TimeShift->setEnabled(false);
        ui.spinBox_TimeValue->setEnabled(false);

        if (ADM_PREVIEW_NONE != admPreview::getPreviewMode())
            slider->setEnabled(false);

        return;
    }

    bool vid, undo, redo, paste, resetA, resetB, canDelete;
    vid = undo = redo = paste = resetA = resetB = canDelete = false;
    if (avifileinfo)
        vid = true; // a video is loaded

    int n = ActionsAvailableWhenFileLoaded.size();
    for (int i = 0; i < n; i++)
        ActionsAvailableWhenFileLoaded[i]->setEnabled(vid);

    int ntb = ButtonsAvailableWhenFileLoaded.size();
    for (int i = 0; i < ntb; i++)
        ButtonsAvailableWhenFileLoaded[i]->setEnabled(vid);

    int npb = PushButtonsAvailableWhenFileLoaded.size();
    for (int i = 0; i < npb; i++)
        PushButtonsAvailableWhenFileLoaded[i]->setEnabled(vid);

#define ENABLE(x, y, z)                                                                                                \
    {                                                                                                                  \
        QAction *a = findAction(&myMenu##x, y);                                                                        \
        if (a)                                                                                                         \
            a->setEnabled(z);                                                                                          \
    }
#define TOOLBAR_ENABLE(x, y)                                                                                           \
    {                                                                                                                  \
        QAction *a = findActionInToolBar(ui.toolBar, x);                                                               \
        if (a)                                                                                                         \
            a->setEnabled(y);                                                                                          \
    }
    ENABLE(File, ACT_SAVE_VIDEO, vid && ADM_mx_getNbMuxers()) // disable saving video if there are no muxers
    if (vid)
    {
        undo = video_body->canUndo();
        redo = video_body->canRedo();
        if (video_body->getMarkerAPts())
            resetA = true;
        if (video_body->getMarkerBPts() != video_body->getVideoDuration())
            resetB = true;
        if ((resetA || resetB) && video_body->getMarkerAPts() != video_body->getMarkerBPts())
            canDelete = true;
        paste = !video_body->clipboardEmpty();
    }
    ENABLE(Edit, ACT_Undo, undo)
    ENABLE(Edit, ACT_Redo, redo)
    ENABLE(Edit, ACT_ResetSegments, vid)
    // TODO: Detect that segment layout matches the default one and disable "Reset Edit" then too.
    ENABLE(Edit, ACT_ResetMarkerA, resetA)
    ENABLE(Edit, ACT_ResetMarkerB, resetB)
    ENABLE(Edit, ACT_ResetMarkers, (resetA || resetB))

    ENABLE(Edit, ACT_Cut, canDelete)
    ENABLE(Edit, ACT_Delete, canDelete)
    ENABLE(Edit, ACT_Paste, paste)

    n = ActionsAlwaysAvailable.size();
    for (int i = 0; i < n; i++)
        ActionsAlwaysAvailable[i]->setEnabled(true);

    ui.toolButtonPlay->setIcon(QIcon(MKICON(player_play)));
    ui.menuGo->actions().at(0)->setIcon(QIcon(MKICON(player_play)));

    ui.toolButtonDeleteSelection->setEnabled(canDelete);

    bool haveRecentItems = false;
    if (recentFiles && recentFiles->actions().size())
        haveRecentItems = true;
    if (recentProjects && recentProjects->actions().size())
        haveRecentItems = true;
    ENABLE(Recent, ACT_CLEAR_RECENT, haveRecentItems)
    ENABLE(Recent, ACT_RESTORE_SESSION, A_checkSavedSession(false))

    ui.selectionDuration->setEnabled(vid);
    slider->setEnabled(vid);

    updateCodecWidgetControlsState();
    // actions performed by the code above may result in a window resize event,
    // which in turn may initiate unwanted zoom changes e.g. when stopping playback
    // or loading a video with small dimensions, so ignore just this one resize event
    ignoreResizeEvent = true;
    // en passant reset frame type label if no video is loaded
    if (!vid)
        ui.label_8->setText(QT_TRANSLATE_NOOP("qgui2", "?"));
}

/**
    \fn updateCodecWidgetControlsState
*/
void MainWindow::updateCodecWidgetControlsState(void)
{
    bool b = false;
    // currently only lavc provides some decoder options
    if (avifileinfo && !strcmp(video_body->getVideoDecoderName(), "Lavcodec"))
        b = true;
    // take care of the "Decoder Options" item in the menu "Video"
    ENABLE(Video, ACT_DecoderOption, b)
    // post-processing is available only for software decoding
    b = false;
    if (avifileinfo && strcmp(video_body->getVideoDecoderName(), "VDPAU") &&
        strcmp(video_body->getVideoDecoderName(), "LIBVA") && strcmp(video_body->getVideoDecoderName(), "DXVA2"))
        // VideoToolbox decoder always downloads decoded image immediately
        b = true;
#ifdef USE_LIBPOSTPROC
    ENABLE(Video, ACT_SetPostProcessing, b)
#endif
    // HDR tone mapper settings action in the menu "Video" and the toolbar button
    b = video_body->possibleHdrContent();
    ENABLE(Video, ACT_SetHDRConfig, b)
    actionHDRSeparator->setVisible(b);
    ui.actionHDRSettings->setVisible(b);
    ui.actionHDRSettings->setEnabled(b && !playing && !navigateWhilePlayingState);

    b = false;
    if (ui.comboBoxVideo->currentIndex())
        b = true;
    ui.pushButtonVideoConf->setEnabled(b);
    if (avifileinfo)
    {
        ui.pushButtonVideoFilter->setEnabled(b);
        // take care of the "Filter" item in the menu "Video" as well
        ENABLE(Video, ACT_VIDEO_FILTERS, b)
        ENABLE(Video, ACT_VIDEO_PARTIAL_FILTERS, b)
        ENABLE(Video, ACT_PreviewChanged, b)
        TOOLBAR_ENABLE(ACT_PreviewChanged, b)
    }
    else
    {
        ui.pushButtonVideoFilter->setEnabled(false);
        ENABLE(Video, ACT_VIDEO_FILTERS, false)
        ENABLE(Video, ACT_VIDEO_PARTIAL_FILTERS, false)
        ENABLE(Video, ACT_PreviewChanged, false)
        TOOLBAR_ENABLE(ACT_PreviewChanged, false)
    }

    b = false;
    if (avifileinfo && video_body->getDefaultEditableAudioTrack())
    {
        ENABLE(Audio, ACT_SAVE_AUDIO, true)
        if (ui.comboBoxAudio->currentIndex())
            b = true;
    }
    else
    { // disable "Save Audio" item in the menu "Audio" if we have no audio tracks
        ENABLE(Audio, ACT_SAVE_AUDIO, false)
    }
    ui.pushButtonAudioConf->setEnabled(b);
    ui.pushButtonAudioFilter->setEnabled(b);
    // take care of the "Filter" item in the menu "Audio"
    ENABLE(Audio, ACT_AUDIO_FILTERS, b)
#undef ENABLE
    // reenable the controls below unconditionally after playback
    ui.checkBox_TimeShift->setEnabled(true);
    ui.spinBox_TimeValue->setEnabled(true);
    // disable the "Save" button in the toolbar and the "Configure" button for the output format if we have no muxers
    b = false;
    bool gotMuxers = (bool)ADM_mx_getNbMuxers();
    if (avifileinfo && gotMuxers)
        b = true;
    TOOLBAR_ENABLE(ACT_SAVE_VIDEO, b)
    ui.pushButtonFormatConfigure->setEnabled(gotMuxers);
}

/**
    \fn updateActionShortcuts
*/
void MainWindow::updateActionShortcuts(void)
{
    bool alt = false, swpud = false;
    prefs->get(KEYBOARD_SHORTCUTS_USE_ALTERNATE_KBD_SHORTCUTS, &alt);
    prefs->get(KEYBOARD_SHORTCUTS_SWAP_UP_DOWN_KEYS, &swpud);

    {
        QAction *q;

        q = findAction(&myMenuGo, ACT_PreviousKFrame);
        if (q)
            q->setShortcut(swpud ? Qt::Key_Up : Qt::Key_Down);

        q = findAction(&myMenuGo, ACT_NextKFrame);
        if (q)
            q->setShortcut(swpud ? Qt::Key_Down : Qt::Key_Up);

        q = findAction(&myMenuGo, ACT_PrevCutPoint);
        if (q)
            q->setShortcut(Qt::SHIFT | (swpud ? Qt::Key_Up : Qt::Key_Down));

        q = findAction(&myMenuGo, ACT_NextCutPoint);
        if (q)
            q->setShortcut(Qt::SHIFT | (swpud ? Qt::Key_Down : Qt::Key_Up));

        q = findAction(&myMenuGo, ACT_Back1Mn);
        if (q)
            q->setShortcut(Qt::CTRL | (swpud ? Qt::Key_Up : Qt::Key_Down));

        q = findAction(&myMenuGo, ACT_Forward1Mn);
        if (q)
            q->setShortcut(Qt::CTRL | (swpud ? Qt::Key_Down : Qt::Key_Up));
    }

    std::vector<MenuEntry *> defaultShortcuts;

    for (int i = 0; i < myMenuEdit.size(); i++)
    {
        MenuEntry *m = &myMenuEdit[i];
        // The separator is number 7, but this is a bit more readable
        if (m->type != MENU_ACTION)
            continue;
        if (m->event == ACT_PREFERENCES)
            break;
        switch (m->event)
        {
        case ACT_Delete:
        case ACT_MarkA:
        case ACT_MarkB:
        case ACT_ResetMarkerA:
        case ACT_ResetMarkerB:
        case ACT_ResetMarkers:
            defaultShortcuts.push_back(m);
            break;
        default:
            break;
        }
    }

    for (int i = 0; i < myMenuGo.size(); i++)
    {
        MenuEntry *m = &myMenuGo[i];
        if (m->type != MENU_ACTION)
            continue;
        if (m->event == ACT_SelectTime)
            break;
        switch (m->event)
        {
        case ACT_Begin:
        case ACT_End:
        case ACT_GotoMarkA:
        case ACT_GotoMarkB:
            defaultShortcuts.push_back(m);
            break;
        default:
            break;
        }
    }

    int n = defaultShortcuts.size();
    for (int i = 0; i < n; i++)
    {
        MenuEntry *m = defaultShortcuts.at(i);
        if (!m)
            continue;
        QAction *a = (QAction *)m->cookie;
        if (!a)
            continue;
        if (alt)
        {
            std::string sc = "";
            switch (m->event)
            {
            case ACT_MarkA:
                prefs->get(KEYBOARD_SHORTCUTS_ALT_MARK_A, sc);
                break;
            case ACT_MarkB:
                prefs->get(KEYBOARD_SHORTCUTS_ALT_MARK_B, sc);
                break;
            case ACT_ResetMarkerA:
                prefs->get(KEYBOARD_SHORTCUTS_ALT_RESET_MARK_A, sc);
                break;
            case ACT_ResetMarkerB:
                prefs->get(KEYBOARD_SHORTCUTS_ALT_RESET_MARK_B, sc);
                break;
            case ACT_ResetMarkers:
                prefs->get(KEYBOARD_SHORTCUTS_ALT_RESET_MARKERS, sc);
                break;
            case ACT_GotoMarkA:
                prefs->get(KEYBOARD_SHORTCUTS_ALT_GOTO_MARK_A, sc);
                break;
            case ACT_GotoMarkB:
                prefs->get(KEYBOARD_SHORTCUTS_ALT_GOTO_MARK_B, sc);
                break;
            case ACT_Begin:
                prefs->get(KEYBOARD_SHORTCUTS_ALT_BEGIN, sc);
                break;
            case ACT_End:
                prefs->get(KEYBOARD_SHORTCUTS_ALT_END, sc);
                break;
            case ACT_Delete:
                prefs->get(KEYBOARD_SHORTCUTS_ALT_DELETE, sc);
                break;
            default:
                sc = std::string(m->shortCut);
            }
            QString qsc = QString::fromUtf8(sc.c_str());
            a->setShortcut(QKeySequence(qsc));
        }
        else
        {
            QKeySequence s(m->shortCut);
            a->setShortcut(s);
        }
    }

    widgetsUpdateTooltips();
}

/**
    \fn getActionShortcutString
*/
static QString getActionShortcutString(QMenu *menu, std::vector<MenuEntry> *list, Action action)
{
    QString s = " ";
    for (int i = 0; i < menu->actions().size(); i++)
    {
        QAction *a = menu->actions().at(i);
        const MenuEntry *m = getMenuEntryForAction(list, a);
        if (!m)
            continue;
        if (m->type != MENU_ACTION)
            continue;
        if (m->event != action)
            continue;
        QKeySequence seq = a->shortcut();
        s = seq.toString().toUpper();
        break;
    }
    return s;
}

/**
    \fn widgetsUpdateTooltips
    \brief Update tooltips showing tunable action shortcuts in the navigation and selection widgets
*/
void MainWindow::widgetsUpdateTooltips(void)
{
    QString tt;

#define SHORTCUT(x, y) QString(" [") + getActionShortcutString(ui.menu##y, &myMenu##y, x) + QString("]");

    tt = QT_TRANSLATE_NOOP("qgui2", "Play/Stop");
    tt += SHORTCUT(ACT_PlayAvi, Go) ui.toolButtonPlay->setToolTip(tt);

    tt = QT_TRANSLATE_NOOP("qgui2", "Go to previous frame");
    tt += SHORTCUT(ACT_PreviousFrame, Go) ui.toolButtonPreviousFrame->setToolTip(tt);

    tt = QT_TRANSLATE_NOOP("qgui2", "Go to next frame");
    tt += SHORTCUT(ACT_NextFrame, Go) ui.toolButtonNextFrame->setToolTip(tt);

    tt = QT_TRANSLATE_NOOP("qgui2", "Go to previous keyframe");
    tt += SHORTCUT(ACT_PreviousKFrame, Go) ui.toolButtonPreviousIntraFrame->setToolTip(tt);

    tt = QT_TRANSLATE_NOOP("qgui2", "Go to next keyframe");
    tt += SHORTCUT(ACT_NextKFrame, Go) ui.toolButtonNextIntraFrame->setToolTip(tt);

    tt = QT_TRANSLATE_NOOP("qgui2", "Delete selection");
    tt += SHORTCUT(ACT_Delete, Edit) ui.toolButtonDeleteSelection->setToolTip(tt);

    tt = QT_TRANSLATE_NOOP("qgui2", "Set start marker");
    tt += SHORTCUT(ACT_MarkA, Edit) ui.toolButtonSetMarkerA->setToolTip(tt);

    tt = QT_TRANSLATE_NOOP("qgui2", "Set end marker");
    tt += SHORTCUT(ACT_MarkB, Edit) ui.toolButtonSetMarkerB->setToolTip(tt);

    tt = QT_TRANSLATE_NOOP("qgui2", "Go to previous cut point");
    tt += SHORTCUT(ACT_PrevCutPoint, Go) ui.toolButtonPreviousCutPoint->setToolTip(tt);

    tt = QT_TRANSLATE_NOOP("qgui2", "Go to next cut point");
    tt += SHORTCUT(ACT_NextCutPoint, Go) ui.toolButtonNextCutPoint->setToolTip(tt);

    // go to black frame tooltips are static, the actions don't have shortcuts

    tt = QT_TRANSLATE_NOOP("qgui2", "Go to first frame");
    tt += SHORTCUT(ACT_Begin, Go) ui.toolButtonFirstFrame->setToolTip(tt);

    tt = QT_TRANSLATE_NOOP("qgui2", "Go to last frame");
    tt += SHORTCUT(ACT_End, Go) ui.toolButtonLastFrame->setToolTip(tt);

    tt = QT_TRANSLATE_NOOP("qgui2", "Go to marker A");
    tt += SHORTCUT(ACT_GotoMarkA, Go) ui.pushButtonJumpToMarkerA->setToolTip(tt);

    tt = QT_TRANSLATE_NOOP("qgui2", "Go to marker B");
    tt += SHORTCUT(ACT_GotoMarkB, Go) ui.pushButtonJumpToMarkerB->setToolTip(tt);

    QString backtext, forwardtext, hint = "\n";
    Action actBack, actForward;

    navigateByTimeButtonsState %= 4;

    switch (navigateByTimeButtonsState)
    {
    case 0:
        ui.toolButtonBackOneMinute->setIcon(QIcon(MKICON(backward1mn)));
        ui.toolButtonForwardOneMinute->setIcon(QIcon(MKICON(forward1mn)));
        actBack = ACT_Back1Mn;
        actForward = ACT_Forward1Mn;
        backtext = QT_TRANSLATE_NOOP("qgui2", "Backward one minute");
        forwardtext = QT_TRANSLATE_NOOP("qgui2", "Forward one minute");
        break;
    case 1:
        ui.toolButtonBackOneMinute->setIcon(QIcon(MKICON(backward1s)));
        ui.toolButtonForwardOneMinute->setIcon(QIcon(MKICON(forward1s)));
        actBack = ACT_Back1Second;
        actForward = ACT_Forward1Second;
        backtext = QT_TRANSLATE_NOOP("qgui2", "Backward 1 second");
        forwardtext = QT_TRANSLATE_NOOP("qgui2", "Forward 1 second");
        break;
    case 2:
        ui.toolButtonBackOneMinute->setIcon(QIcon(MKICON(backward2s)));
        ui.toolButtonForwardOneMinute->setIcon(QIcon(MKICON(forward2s)));
        actBack = ACT_Back2Seconds;
        actForward = ACT_Forward2Seconds;
        backtext = QT_TRANSLATE_NOOP("qgui2", "Backward 2 seconds");
        forwardtext = QT_TRANSLATE_NOOP("qgui2", "Forward 2 seconds");
        break;
    case 3:
        ui.toolButtonBackOneMinute->setIcon(QIcon(MKICON(backward4s)));
        ui.toolButtonForwardOneMinute->setIcon(QIcon(MKICON(forward4s)));
        actBack = ACT_Back4Seconds;
        actForward = ACT_Forward4Seconds;
        backtext = QT_TRANSLATE_NOOP("qgui2", "Backward 4 seconds");
        forwardtext = QT_TRANSLATE_NOOP("qgui2", "Forward 4 seconds");
        break;
    default:
        ADM_assert(0);
        break;
    }

    modifyTranslationTable("toolButtonBackOneMinute", actBack);
    modifyTranslationTable("toolButtonForwardOneMinute", actForward);

    hint += QT_TRANSLATE_NOOP("qgui2", "Rotate mouse wheel to switch mode");

    tt = backtext;
    tt += SHORTCUT(actBack, Go) tt += hint;
    ui.toolButtonBackOneMinute->setToolTip(tt);

    tt = forwardtext;
    tt += SHORTCUT(actForward, Go) tt += hint;
    ui.toolButtonForwardOneMinute->setToolTip(tt);
}

/**
    \fn     restoreDefaultWidgetState
    \brief  Show all dock widgets and move toolbar to the default area
*/
void MainWindow::restoreDefaultWidgetState(bool b)
{
    ui.codecWidget->setVisible(true);
    ui.navigationWidget->setVisible(true);
    ui.selectionWidget->setVisible(true);
    ui.volumeWidget->setVisible(true);
    ui.audioMetreWidget->setVisible(true);
    ui.toolBar->setVisible(true);
    addStatusBar();

    syncToolbarsMenu();
    updateZoomIndicator();

    addToolBar(ui.toolBar);

    if (!playing)
        setZoomToFit();
}

/**
    \fn     setDefaultThemeSlot
    \brief  Set default theme and update settings.
*/
void MainWindow::setDefaultThemeSlot(bool b)
{
    UNUSED_ARG(b);

    if (NULL != getenv("ADM_QT_STYLE_VERBOSE"))
    {
        QString styleName = "unknown style";
        QStyle *currentStyle = qApp->style();
        if (currentStyle)
            styleName = currentStyle->objectName();

        ADM_info("Slot triggered, current style: %s\n", styleName.toUtf8().constData());
    }

    QStyle *style = QStyleFactory::create(defaultStyle);
    if (!style)
    {
        ADM_warning("Invalid Qt style name \"%s\"\n", defaultStyle.toUtf8().constData());
        return;
    }
    QPalette pal; // empty palette to restore native style colors
    qApp->setStyle(style);
    qApp->setPalette(pal);
    ui.currentTime->setTextMargins(0, 0, 0, 0);

#ifdef BROKEN_PALETTE_PROPAGATION
#define PROPAGATE_PALETTE(x)                                                                                           \
    ui.checkBox_TimeShift->setPalette(x);                                                                              \
    ui.spinBox_TimeValue->setPalette(x);                                                                               \
    ui.currentTime->setPalette(x);                                                                                     \
    ui.menuFile->setPalette(x);                                                                                        \
    ui.menuRecent->setPalette(x);                                                                                      \
    ui.menuEdit->setPalette(x);                                                                                        \
    ui.menuView->setPalette(x);                                                                                        \
    ui.menuToolbars->setPalette(x);                                                                                    \
    ui.menuThemes->setPalette(x);                                                                                      \
    ui.menuVideo->setPalette(x);                                                                                       \
    ui.menuAudio->setPalette(x);                                                                                       \
    ui.menuAuto->setPalette(x);                                                                                        \
    ui.menuTools->setPalette(x);                                                                                       \
    ui.menuGo->setPalette(x);                                                                                          \
    ui.menuCustom->setPalette(x);                                                                                      \
    ui.menuHelp->setPalette(x);                                                                                        \
    ui.toolBar->setPalette(x);                                                                                         \
    if (recentFiles)                                                                                                   \
        recentFiles->setPalette(x);                                                                                    \
    if (recentProjects)                                                                                                \
        recentProjects->setPalette(x);                                                                                 \
    for (int i = 0; i < subMenus.size(); i++)                                                                          \
    {                                                                                                                  \
        QMenu *m = subMenus.at(i);                                                                                     \
        if (m)                                                                                                         \
            m->setPalette(x);                                                                                          \
    }                                                                                                                  \
    ui.comboBoxVideo->view()->setPalette(x);                                                                           \
    ui.comboBoxAudio->view()->setPalette(x);                                                                           \
    ui.comboBoxFormat->view()->setPalette(x);                                                                          \
    ui.pushButtonVideoConf->setPalette(x);                                                                             \
    ui.pushButtonVideoFilter->setPalette(x);                                                                           \
    ui.pushButtonAudioConf->setPalette(x);                                                                             \
    ui.pushButtonAudioFilter->setPalette(x);                                                                           \
    ui.pushButtonFormatConfigure->setPalette(x);                                                                       \
    ui.pushButtonTime->setPalette(x);                                                                                  \
    ui.pushButtonJumpToMarkerA->setPalette(x);                                                                         \
    ui.pushButtonJumpToMarkerB->setPalette(x);

    PROPAGATE_PALETTE(pal)
#endif
#ifdef _WIN32
    setTimeDisplaySize();
#endif
    defaultThemeAction->setChecked(true);
    lightThemeAction->setChecked(false);
    darkThemeAction->setChecked(false);

    QSettings *qset = qtSettingsCreate();
    if (qset)
    {
        qset->beginGroup("MainWindow");
        qset->setValue("theme", ADM_QT_THEME_DEFAULT);
        qset->endGroup();
        delete qset;
        qset = NULL;
    }
}
/**
    \fn     setLightTheme
    \brief  Set default fusion theme
*/
void MainWindow::setLightTheme(void)
{
    mySetStyle();
    QPalette lightPalette;
    lightPalette.setColor(QPalette::Window, QColor(239, 239, 239));
    lightPalette.setColor(QPalette::WindowText, QColor(0, 0, 0));
    lightPalette.setColor(QPalette::Base, QColor(255, 255, 255));
    lightPalette.setColor(QPalette::AlternateBase, QColor(247, 247, 247));
    lightPalette.setColor(QPalette::ToolTipBase, QColor(255, 255, 220));
    lightPalette.setColor(QPalette::ToolTipText, QColor(0, 0, 0));
    lightPalette.setColor(QPalette::Text, QColor(0, 0, 0));
    lightPalette.setColor(QPalette::Button, QColor(239, 239, 239));
    lightPalette.setColor(QPalette::ButtonText, QColor(0, 0, 0));
    lightPalette.setColor(QPalette::BrightText, Qt::white);
    lightPalette.setColor(QPalette::Link, QColor(48, 140, 198));

    lightPalette.setColor(QPalette::Highlight, QColor(48, 140, 198));
    lightPalette.setColor(QPalette::HighlightedText, Qt::white);

    lightPalette.setColor(QPalette::Active, QPalette::Button, QColor(239, 239, 239));
    lightPalette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(190, 190, 190));
    lightPalette.setColor(QPalette::Disabled, QPalette::WindowText, QColor(190, 190, 190));
    lightPalette.setColor(QPalette::Disabled, QPalette::Text, QColor(190, 190, 190));
    lightPalette.setColor(QPalette::Disabled, QPalette::Light, QColor(255, 255, 255));

    qApp->setPalette(lightPalette);
#ifdef BROKEN_PALETTE_PROPAGATION
    PROPAGATE_PALETTE(lightPalette)
#endif
#ifdef _WIN32
    setTimeDisplaySize();
#endif
    defaultThemeAction->setChecked(false);
    lightThemeAction->setChecked(true);
    darkThemeAction->setChecked(false);
}

/**
    \fn     setLightThemeSlot
    \brief  Set default fusion theme and update settings.
*/
void MainWindow::setLightThemeSlot(bool b)
{
    UNUSED_ARG(b);

    setLightTheme();

    QSettings *qset = qtSettingsCreate();
    if (qset)
    {
        qset->beginGroup("MainWindow");
        qset->setValue("theme", ADM_QT_THEME_LIGHT);
        qset->endGroup();
        delete qset;
        qset = NULL;
    }
}

/**
    \fn     setDarkTheme
    \brief  Set dark fusion theme
*/
void MainWindow::setDarkTheme(void)
{
    mySetStyle();
    QPalette darkPalette;
    darkPalette.setColor(QPalette::Window, QColor(32, 32, 32));
    darkPalette.setColor(QPalette::WindowText, QColor(234, 234, 234));
    darkPalette.setColor(QPalette::Base, QColor(55, 55, 55));
    darkPalette.setColor(QPalette::AlternateBase, QColor(42, 42, 42));
    darkPalette.setColor(QPalette::ToolTipBase, QColor(42, 130, 218));
    darkPalette.setColor(QPalette::ToolTipText, QColor(234, 234, 234));
    darkPalette.setColor(QPalette::Text, QColor(234, 234, 234));
    darkPalette.setColor(QPalette::Button, QColor(42, 42, 42));
    darkPalette.setColor(QPalette::ButtonText, QColor(234, 234, 234));
    darkPalette.setColor(QPalette::BrightText, Qt::red);
    darkPalette.setColor(QPalette::Link, QColor(42, 130, 218));

    darkPalette.setColor(QPalette::Highlight, QColor(42, 130, 218));
    darkPalette.setColor(QPalette::HighlightedText, Qt::black);

    darkPalette.setColor(QPalette::Active, QPalette::Button, QColor(128, 128, 128).darker());
    darkPalette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(128, 128, 128));
    darkPalette.setColor(QPalette::Disabled, QPalette::WindowText, QColor(128, 128, 128));
    darkPalette.setColor(QPalette::Disabled, QPalette::Text, QColor(128, 128, 128));
    darkPalette.setColor(QPalette::Disabled, QPalette::Light, QColor(42, 42, 42));

    qApp->setPalette(darkPalette);
#ifdef BROKEN_PALETTE_PROPAGATION
    PROPAGATE_PALETTE(darkPalette)
#endif
#ifdef _WIN32
    setTimeDisplaySize();
#endif
    defaultThemeAction->setChecked(false);
    lightThemeAction->setChecked(false);
    darkThemeAction->setChecked(true);
}

/**
    \fn     setDarkThemeSlot
    \brief  Set dark fusion theme and update settings.
*/
void MainWindow::setDarkThemeSlot(bool b)
{
    UNUSED_ARG(b);

    setDarkTheme();

    QSettings *qset = qtSettingsCreate();
    if (qset)
    {
        qset->beginGroup("MainWindow");
        qset->setValue("theme", ADM_QT_THEME_DARK);
        qset->endGroup();
        delete qset;
        qset = NULL;
    }
}

/**
 * \fn checkChanged
 * \brief the checkbox protecting timeshift value has changed
 * @param state
 */
void MainWindow::checkChanged(int state)
{
    bool b = true;
    if (state)
        b = true;
    else
        b = false;
    ui.spinBox_TimeValue->setEnabled(b);
    timeChanged(0);
}
/**
    \fn timeChanged
    \brief Called whenever timeshift is on/off'ed or value changes
*/
void MainWindow::timeChanged(int)
{
    sendAction(ACT_TimeShift);
}
/**
    \fn searchMenu
*/
void MainWindow::searchMenu(QAction *action, MenuEntry *menu, int nb)
{
    for (int i = 0; i < nb; i++)
    {
        MenuEntry *m = menu + i;
        if (m->cookie == (void *)action)
        {
            sendAction(m->event);
        }
    }
}

/**
    \fn searchFileMenu
*/
#define MKMENU(name)                                                                                                   \
    void MainWindow::search##name##Menu(QAction *action)                                                               \
    {                                                                                                                  \
        searchMenu(action, &myMenu##name[0], myMenu##name.size());                                                     \
    }

MKMENU(File)
MKMENU(Edit)
MKMENU(Recent)
MKMENU(View)
MKMENU(Tool)
MKMENU(Go)
// MKMENU(Custom)
MKMENU(Audio)
MKMENU(Video)
MKMENU(Help)

/*
      We receive a button press event
*/
void MainWindow::buttonPressed(void)
{
    // Receveid a key press Event, look into table..
    QObject *obj = sender();
    if (!obj)
        return;
    QString me(obj->objectName());

    Action action = searchTranslationTable(qPrintable(me));

    if (action != ACT_DUMMY)
        sendAction(action);
}
void MainWindow::toolButtonPressed(bool i)
{
    buttonPressed();
}
#ifdef ENABLE_EVENT_FILTER
bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    QKeyEvent *keyEvent;
    bool swpud = false;

    switch (event->type())
    {
    case QEvent::KeyPress:
        keyEvent = (QKeyEvent *)event;
        prefs->get(KEYBOARD_SHORTCUTS_SWAP_UP_DOWN_KEYS, &swpud);
        //            if (watched == slider)
        {
            switch (keyEvent->key())
            {
            case Qt::Key_Left:
                if ((keyEvent->modifiers() & Qt::ShiftModifier) && (keyEvent->modifiers() & Qt::ControlModifier))
                    sendAction(ACT_Back4Seconds);
                else if (keyEvent->modifiers() & Qt::ShiftModifier)
                    sendAction(ACT_Back1Second);
                else if (keyEvent->modifiers() & Qt::ControlModifier)
                    sendAction(ACT_Back2Seconds);
                else
                    sendAction(ACT_PreviousFrame);

                return true;
            case Qt::Key_Right:
                if ((keyEvent->modifiers() & Qt::ShiftModifier) && (keyEvent->modifiers() & Qt::ControlModifier))
                    sendAction(ACT_Forward4Seconds);
                else if (keyEvent->modifiers() & Qt::ShiftModifier)
                    sendAction(ACT_Forward1Second);
                else if (keyEvent->modifiers() & Qt::ControlModifier)
                    sendAction(ACT_Forward2Seconds);
                else
                    sendAction(ACT_NextFrame);

                return true;
            case Qt::Key_Up:
                if (keyEvent->modifiers() & Qt::ControlModifier)
                {
                    if (!swpud)
                        sendAction(ACT_Forward1Mn);
                    else
                        sendAction(ACT_Back1Mn);
                }
                else if (keyEvent->modifiers() & Qt::ShiftModifier)
                {
                    if (!swpud)
                        sendAction(ACT_NextCutPoint);
                    else
                        sendAction(ACT_PrevCutPoint);
                }
                else
                {
                    if (!swpud)
                        sendAction(ACT_NextKFrame);
                    else
                        sendAction(ACT_PreviousKFrame);
                }
                return true;
            case Qt::Key_Down:
                if (keyEvent->modifiers() & Qt::ControlModifier)
                {
                    if (!swpud)
                        sendAction(ACT_Back1Mn);
                    else
                        sendAction(ACT_Forward1Mn);
                }
                else if (keyEvent->modifiers() & Qt::ShiftModifier)
                {
                    if (!swpud)
                        sendAction(ACT_PrevCutPoint);
                    else
                        sendAction(ACT_NextCutPoint);
                }
                else
                {
                    if (!swpud)
                        sendAction(ACT_PreviousKFrame);
                    else
                        sendAction(ACT_NextKFrame);
                }
                return true;
            case Qt::Key_C:
                if ((keyEvent->modifiers() & Qt::ShiftModifier) && (keyEvent->modifiers() & Qt::ControlModifier))
                    currentTimeToClipboard();
                return true;
            case Qt::Key_Shift:
                shiftKeyHeld = 1;
                break;
            case Qt::Key_Control:
                ctrlKeyHeld = 1;
                break;

            case Qt::Key_PageUp:
                if (keyEvent->modifiers() & Qt::ControlModifier)
                    sendAction(ACT_MarkA);
                else
                    sendAction(ACT_GotoMarkA);
                return true;
            case Qt::Key_PageDown:
                if (keyEvent->modifiers() & Qt::ControlModifier)
                    sendAction(ACT_MarkB);
                else
                    sendAction(ACT_GotoMarkB);
                return true;
            default:
                break;
            }
        }
        /* else */ if (keyEvent->key() == Qt::Key_Space && avifileinfo)
        {
            sendAction(ACT_PlayAvi);
            return true;
        }

        break;
    case QEvent::Resize:
        if (watched == QuiMainWindows)
        {
            QSize os = static_cast<QResizeEvent *>(event)->oldSize();
            adjustZoom(os.width(), os.height());
            break;
        }
        if (watched == ui.sliderPlaceHolder)
        {
            thumbSlider->resize(ui.sliderPlaceHolder->width(), 16);
            thumbSlider->move(0, (ui.sliderPlaceHolder->height() - thumbSlider->height()) / 2);
        }
        break;

    case QEvent::KeyRelease:
        keyEvent = (QKeyEvent *)event;

        if (keyEvent->key() == Qt::Key_Shift)
            shiftKeyHeld = 0;

        if (keyEvent->key() == Qt::Key_Control)
            ctrlKeyHeld = 0;

        break;
    case QEvent::ShortcutOverride:
        ctrlKeyHeld = 0;
        break;
    case QEvent::User:
        this->openFiles(((FileDropEvent *)event)->files);
        break;
    case QEvent::Wheel:
        if ((watched == ui.toolButtonBackOneMinute) || (watched == ui.toolButtonForwardOneMinute))
        {
            QWheelEvent *e = (QWheelEvent *)event;
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
            if (e->delta() == 0)
            {
            }
            else if (e->delta() > 0)
#else
            if (e->angleDelta().ry() == 0)
            {
            }
            else if (e->angleDelta().ry() > 0)
#endif
            {
                navigateByTimeButtonsState += 1;
                navigateByTimeButtonsState %= 4;
                updateActionShortcuts();
            }
            else
            {
                navigateByTimeButtonsState -= 1;
                navigateByTimeButtonsState %= 4;
                updateActionShortcuts();
            }
        }
        break;
    default:
        break;
    }

    return QObject::eventFilter(watched, event);
}
#endif
void MainWindow::mousePressEvent(QMouseEvent *event)
{
    this->setFocus(Qt::OtherFocusReason);
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasFormat("text/uri-list"))
    {
        event->setDropAction(Qt::CopyAction);
        event->accept();
    }
}

void MainWindow::dropEvent(QDropEvent *event)
{
    if (event->mimeData()->hasUrls())
    {
        event->setDropAction(Qt::CopyAction);
        QCoreApplication::postEvent(this, new FileDropEvent(event->mimeData()->urls()));
        event->accept();
    }
}

/**
 *  \fn windowStateToString
 */
static const char *windowStateToString(const Qt::WindowStates s)
{
    if (s & Qt::WindowMinimized)
        return "minimized";
    else if (s & Qt::WindowMaximized)
        return "maximized";
    else if (s & Qt::WindowFullScreen)
        return "fullscreen";
    else
        return "normal";
}

// If a video was loaded or zoom changed while the main window was maximized,
// unmaximizing the window restores it to dimensions not necessarily matching
// the actual dimensions of the video frame. Catch the window state change event
// and resize the window when appropriate.
void MainWindow::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::WindowStateChange && false == QuiMainWindows->isMinimized())
    {
        QWindowStateChangeEvent *ev = static_cast<QWindowStateChangeEvent *>(event);
        const Qt::WindowStates old = ev->oldState();
        const Qt::WindowStates cur = QuiMainWindows->windowState();
        ADM_info("Window change event: %s -> %s\n", windowStateToString(old), windowStateToString(cur));

        if (!(old & Qt::WindowMinimized)) // Don't do anything on restore from minimized state.
        {
            if (old & Qt::WindowMaximized)
            {
                if (UI_getNeedsResizingFlag())
                {
                    uint32_t w = ui.frame_video->width();
                    uint32_t h = ui.frame_video->height();
                    UI_resize(w, h);
                    UI_setNeedsResizingFlag(false);
                }
                else
                {
                    setZoomToFit();
                }
            }
            // Always adjust zoom on maximize from normal state.
            if (!(old & Qt::WindowMaximized) && QuiMainWindows->isMaximized())
            {
                ignoreResizeEvent = false;
                adjustZoom(0, 0);
            }
        }
    }
    QWidget::changeEvent(event);
}

/**
 *  \fn adjustZoom
 */
bool MainWindow::adjustZoom(int oldWidth, int oldHeight)
{
    if (blockZoomChanges || playing || !avifileinfo)
        return false;
    if (ignoreResizeEvent)
    {
        ignoreResizeEvent = false;
        return false;
    }

    uint32_t reqw, reqh;
    calcDockWidgetDimensions(reqw, reqh);

    if (QuiMainWindows->width() <= reqw)
        return false;

    uint32_t availw = QuiMainWindows->width() - reqw;

    if (QuiMainWindows->height() <= reqh)
        return false;

    uint32_t availh = QuiMainWindows->height() - reqh;

    uint32_t w = avifileinfo->width;
    uint32_t h = avifileinfo->height;
    if (!w || !h)
        return false;
    // We've got the available space and the video resolution,
    // now calculate the zoom to fit the video into this space.
    float widthRatio = (float)availw / (float)w;
    float heightRatio = (float)availh / (float)h;
    float zoom = (widthRatio < heightRatio ? widthRatio : heightRatio);

    // Detect if the zoom has been likely set automatically on loading a new video or
    // changed using a keyboard shortcut. In this case imitate physical friction
    // requiring the window to be resized beyond certain threshold first.
    float oldzoom = admPreview::getCurrentZoom();
    if (oldWidth && oldHeight && zoom > oldzoom && actZoomCalled)
    {
        if (QuiMainWindows->width() > oldWidth)
            threshold -= QuiMainWindows->width() - oldWidth;
        if (QuiMainWindows->height() > oldHeight)
            threshold -= QuiMainWindows->height() - oldHeight;
        if (threshold > 0)
            return false;
        if (threshold < 0)
            threshold = 0;
    }

    if (zoom > oldzoom + .001 || zoom < oldzoom - .001)
    {
        blockResizing = true;
        admPreview::setMainDimension(w, h, zoom);
        actZoomCalled = false;
        admPreview::samePicture(); // required at least for VDPAU
        updateZoomIndicator();
        blockResizing = false;

        return true;
    }
    return false;
}

/**
 *  \fn updateZoomIndicator
 *  \brief Display zoom level in the statusbar.
 */
void MainWindow::updateZoomIndicator(void)
{
    if (!avifileinfo)
        return;

    float percent = admPreview::getCurrentZoom();
    if (percent < 0)
        return;
    percent *= 100;
    percent += 0.49;
    updateStatusBarZoomInfo((int)percent);
}

/**
 *  \fn toolbarOrientationChangedSlot
 */
void MainWindow::toolbarOrientationChangedSlot(Qt::Orientation orientation)
{
    UNUSED_ARG(orientation);
}

/**
 *  \fn setZoomToFit
 *  \brief adjust zoom level to fit video into available space at current window size
 */
void MainWindow::setZoomToFit(void)
{
    ignoreResizeEvent = false;
    adjustZoom(0, 0);
    UI_setNeedsResizingFlag(false);
}

void MainWindow::openFiles(QList<QUrl> urlList)
{
    QFileInfo info;

    for (int fileIndex = 0; fileIndex < urlList.size(); fileIndex++)
    {
        QString fileName = urlList[fileIndex].toLocalFile();
        QFileInfo info(fileName);

        if (info.isFile())
        {
            if (avifileinfo)
                A_appendVideo(fileName.toUtf8().constData());
            else
                A_openVideo(fileName.toUtf8().constData());
            // Set lastdir_read on drag'n'drop here instead of centrally in
            // A_openVideo or in A_appendVideo to better deal with situations
            // where videos are loaded from a project script in a different location.
            // Otherwise lastdir_read is managed by the file selection dialog.
            admCoreUtils::setLastReadFolder(std::string(fileName.toUtf8().constData()));
        }
    }
}

/**
    \fn calcDockWidgetDimensions
    \brief calculate the total width and height occupied by the toolbar and the codec, navigation etc. dock widgets
*/
void MainWindow::calcDockWidgetDimensions(uint32_t &width, uint32_t &height)
{
    uint32_t reqw = 18; // 2 x 9px margin
    if (ui.codecWidget->isVisible())
        reqw += ui.codecWidget->frameSize().width() + 6; // with codec widget visible a small extra margin is necessary
    if (ui.toolBar->orientation() == Qt::Vertical && ui.toolBar->isVisible() && false == ui.toolBar->isFloating())
        reqw += ui.toolBar->frameSize().width();
    width = reqw;

    uint32_t reqh = 18; // 2 x 9px margin
    if (ui.menubar->isVisible())
        reqh += ui.menubar->height();
    if (ui.toolBar->isVisible() && false == ui.toolBar->isFloating() && ui.toolBar->orientation() == Qt::Horizontal)
        reqh += ui.toolBar->frameSize().height();
    if (ui.navigationWidget->isVisible() || ui.selectionWidget->isVisible() || ui.volumeWidget->isVisible() ||
        ui.audioMetreWidget->isVisible())
        reqh += ui.navigationWidget->frameSize().height();
    if (statusBarWidget)
        reqh += statusBarWidget->frameSize().height();
    height = reqh;
}

/**
    \fn setResizeThreshold
*/
void MainWindow::setResizeThreshold(int value)
{
    threshold = value;
}

/**
    \fn setActZoomCalledFlag
*/
void MainWindow::setActZoomCalledFlag(bool called)
{
    actZoomCalled = called;
}

/**
    \fn setBlockZoomChangesFlag
*/
void MainWindow::setBlockZoomChangesFlag(bool block)
{
    blockZoomChanges = block;
}

/**
    \fn getBlockResizingFlag
*/
bool MainWindow::getBlockResizingFlag(void)
{
    return blockResizing;
}

/**
    \fn setBlockResizingFlag
*/
void MainWindow::setBlockResizingFlag(bool block)
{
    blockResizing = block;
}

/**
    \fn volumeWidgetOperational
*/
void MainWindow::volumeWidgetOperational(void)
{
    // Disable the volume widget if the audio device doesn't support setting volume
    ui.volumeWidget->setEnabled(AVDM_hasVolumeControl());
}

/**
    \fn syncToolbarsMenu
    \brief Make sure only visible widgets have check marks
           in the Toolbars submenu of the View menu.
*/
void MainWindow::syncToolbarsMenu(void)
{
#define EXPAND(x) ui.x##Widget
#define CHECKMARK(x, y) ui.menuToolbars->actions().at(x)->setChecked(EXPAND(y)->isVisible());
    CHECKMARK(0, audioMetre)
    CHECKMARK(1, codec)
    CHECKMARK(2, navigation)
    CHECKMARK(3, selection)
    CHECKMARK(4, volume)
    ui.menuToolbars->actions().at(5)->setChecked(ui.toolBar->isVisible());
    ui.menuToolbars->actions().at(6)->setChecked(statusBarEnabled());
#undef CHECKMARK
#undef EXPAND
}

/**
    \fn setStatusBarEnabled
    \brief public slot for setStatusBarEnabled
*/
void MainWindow::setStatusBarEnabled(bool enabled)
{
    if (enabled)
        addStatusBar();
    else
        removeStatusBar();
}

/**
    \fn statusBarTimerTimeout
    \brief private slot for statusBarTimerTimeout
*/
void MainWindow::statusBarTimerTimeout(void)
{
    updateStatusBarInfo();
}

/**
    \fn statusBarEnabled
*/
bool MainWindow::statusBarEnabled(void)
{
    return (statusBarWidget != NULL);
}

/**
    \fn addStatusBar
    \brief Add status bar to the main window
*/
void MainWindow::addStatusBar(void)
{
    if (statusBarWidget)
        return;
    statusBarWidget = new QStatusBar(this);
    statusBarWidget->setSizeGripEnabled(false);

    statusBarInfo = new QLabel("");
    statusBarWidget->addWidget(statusBarInfo);
    statusBarMessage = new QLabel("");
    statusBarWidget->addWidget(statusBarMessage);

    statusBarWidget->setContentsMargins(4, 0, 4, 0);

    this->setStatusBar(statusBarWidget);
    updateStatusBarInfo();
}

/**
    \fn removeStatusBar
    \brief Remove status bar from the main window
*/
void MainWindow::removeStatusBar(void)
{
    if (!statusBarWidget)
        return;

    this->setStatusBar(NULL); // Qt should take care of deleting nested objects

    statusBarTimer.stop();
    statusBarWidget = NULL;
    statusBarInfo = NULL;
    statusBarMessage = NULL;
}

/**
    \fn updateStatusBarInfo
*/
void MainWindow::updateStatusBarInfo(void)
{
    if (!statusBarWidget)
        return;
    QString s = QString("");
    if (avifileinfo)
    {
        s += QString(QT_TRANSLATE_NOOP("qgui2", "Input: %1x%2, %3fps  |  Decoder: %4  |  Display: %5  |  Zoom: %6%"))
                 .arg(avifileinfo->width)
                 .arg(avifileinfo->height)
                 .arg(avifileinfo->fps1000 / 1000.0)
                 .arg(statusBarInfo_Decoder)
                 .arg(statusBarInfo_Display)
                 .arg(statusBarInfo_Zoom);
    }
    else
    {
        s += QString(QT_TRANSLATE_NOOP("qgui2", "No file loaded"));
    }

    if (statusBarInfo)
        statusBarInfo->setText(s);
    if (statusBarMessage)
        statusBarMessage->clear();
    statusBarTimer.stop();
}

/**
    \fn updateStatusBarDisplayInfo
*/
void MainWindow::updateStatusBarDisplayInfo(const char *display)
{
    statusBarInfo_Display = QString(display);
    updateStatusBarInfo();
}

/**
    \fn updateStatusBarDecoderInfo
*/
void MainWindow::updateStatusBarDecoderInfo(const char *decoder)
{
    statusBarInfo_Decoder = QString(decoder);
    updateStatusBarInfo();
}

/**
    \fn updateStatusBarZoomInfo
*/
void MainWindow::updateStatusBarZoomInfo(int zoom)
{
    statusBarInfo_Zoom = zoom;
    updateStatusBarInfo();
}

/**
    \fn notifyStatusBar
*/
void MainWindow::notifyStatusBar(int level, const char *lead, const char *msg, int timeout, bool flash)
{
    if (timeout <= 0) // prevent permament message
        timeout = 2500;
    QString s = QString(lead).arg(msg);
#define STATUSBAR_MESSAGE_ICON_SIZE (12)
    if (statusBarInfo)
    {
        statusBarInfo->clear();
        switch (level)
        {
        case 0:
            statusBarInfo->setPixmap(QApplication::style()
                                         ->standardIcon(QStyle::SP_MessageBoxInformation)
                                         .pixmap(STATUSBAR_MESSAGE_ICON_SIZE, STATUSBAR_MESSAGE_ICON_SIZE));
            break;
        case 1:
            statusBarInfo->setPixmap(QApplication::style()
                                         ->standardIcon(QStyle::SP_MessageBoxWarning)
                                         .pixmap(STATUSBAR_MESSAGE_ICON_SIZE, STATUSBAR_MESSAGE_ICON_SIZE));
            break;
        default:
        case 2:
            statusBarInfo->setPixmap(QApplication::style()
                                         ->standardIcon(QStyle::SP_MessageBoxCritical)
                                         .pixmap(STATUSBAR_MESSAGE_ICON_SIZE, STATUSBAR_MESSAGE_ICON_SIZE));
            break;
        }
    }
#undef STATUSBAR_MESSAGE_ICON_SIZE
    if (statusBarMessage)
        statusBarMessage->setText(s);
    if (statusBarWidget)
    {
        statusBarTimer.start(timeout);

        if (flash && (statusBarFlashTimer.remainingTime() <= 0))
        {
            statusBarFlashTimer.start(200);
            statusBarWidget->showMessage(" ", 100);
        }
    }
}

MainWindow::~MainWindow()
{
    renderDestroy(); // make sure render does not have back link to us
    delete thumbSlider;
    thumbSlider = NULL;
}

static const UI_FUNCTIONS_T UI_Hooks = {
    ADM_RENDER_API_VERSION_NUMBER, UI_getWindowInfo, UI_updateDrawWindowSize, UI_rgbDraw, UI_getDrawWidget,
    UI_getPreferredRender

};

static myQApplication *myApplication = NULL;
QApplication *currentQApplication()
{
    return myApplication;
}
/**
 * \fn UI_reset
 * \brief reset
 * @return
 */
bool UI_reset(void)
{
    UI_setVideoCodec(0);
    UI_setAudioCodec(0);
    UI_setCurrentPreview(false);
    return true;
}

/**
    \fn  UI_Init
    \brief First part of UI initialization

*/
int UI_Init(int nargc, char **nargv)
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
    ADM_info("Starting Qt5 GUI...\n");
#else
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    ADM_info("Starting Qt6 GUI...\n");
#else
    ADM_info("Starting Qt4 GUI...\n");
#endif
#endif
    initTranslator();

    global_argc = nargc;
    global_argv = nargv;

    ADM_renderLibInit(&UI_Hooks);
#if !defined(__APPLE__) && QT_VERSION >= QT_VERSION_CHECK(5, 11, 0) && QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    // Despite HiDPI scaling being supported from Qt 5.6 on, important aspects
    // like OpenGL support were fixed only in much later versions.
    // Enabled by default with Qt6.
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps); // allow HiDPI icons; deprecated >= Qt6
#endif
#if !defined(__APPLE__) && !defined(_WIN32) /* Linux */ && QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    // Fix video shown as solid black color with OpenGL display and Qt6.
    QApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
#endif
#if defined(_WIN32) && ((QT_VERSION >= QT_VERSION_CHECK(5, 10, 0)) && (QT_VERSION <= QT_VERSION_CHECK(6, 0, 0)))

    // Hide unhelpful context help buttons on Windows.
    QApplication::setAttribute(Qt::AA_DisableWindowContextHelpButton);
#endif
    myApplication = new myQApplication(global_argc, global_argv);
    myApplication->setDesktopFileName("org.avidemux.Avidemux");
    myApplication->connect(myApplication, SIGNAL(lastWindowClosed()), myApplication, SLOT(quit()));
    myApplication->connect(myApplication, SIGNAL(aboutToQuit()), myApplication, SLOT(cleanup()));
#ifdef __APPLE__
    Q_INIT_RESOURCE(avidemux_osx);
#elif defined(_WIN32)
    Q_INIT_RESOURCE(avidemux_win32);
#else
    Q_INIT_RESOURCE(avidemux);
#endif
    Q_INIT_RESOURCE(filter);

#ifdef USE_CUSTOM_TIME_DISPLAY_FONT
    if (-1 == QFontDatabase::addApplicationFont(":/new/prefix1/fonts/ADM7SEG.ttf"))
        ADM_warning("LCD display font could not be loaded from resource.\n");
#endif
    loadTranslator();

    return 1;
}

uint8_t initGUI(const vector<IScriptEngine *> &scriptEngines)
{
    MainWindow *mw = new MainWindow(scriptEngines);

    bool openglEnabled = false;
#ifdef USE_OPENGL
    prefs->get(FEATURES_ENABLE_OPENGL, &openglEnabled);
    ADM_info("OpenGL enabled at build time, checking whether we should run it... %s.\n", openglEnabled ? "yes" : "no");
#else
    ADM_info("OpenGL: Not enabled at build time.\n");
#endif
    uiIsMaximized = false;
    bool vuMeterIsHidden = false;
    bool statusbarHidden = false;
    QSettings *qset = qtSettingsCreate();
    if (qset)
    {
        qset->beginGroup("MainWindow");
        mw->restoreState(qset->value("windowState").toByteArray());
        uiIsMaximized = qset->value("showMaximized", false).toBool();
        switch (qset->value("theme", ADM_QT_THEME_DEFAULT).toInt())
        {
        case ADM_QT_THEME_LIGHT:
            mw->setLightTheme();
            break;
        case ADM_QT_THEME_DARK:
            mw->setDarkTheme();
            break;
        default:
            break;
        }
        mw->ui.horizontalSlider_2->blockSignals(true);
        mw->ui.horizontalSlider_2->setValue(qset->value("volume", 100).toInt());
        mw->ui.horizontalSlider_2->blockSignals(false);
        statusbarHidden = qset->value("statusbarHidden", false).toBool();
        qset->endGroup();
        // Hack: allow to drop other Qt-specific settings on application restart
        char *dropSettingsOnLaunch = getenv("ADM_QT_DROP_SETTINGS");
        if (dropSettingsOnLaunch && !strcmp("1", dropSettingsOnLaunch))
            qset->clear();
        delete qset;
        qset = NULL;
        // Probing for OpenGL fails if dummyGLWidget parent is hidden.
        // If VU meter is that parent, delay hiding it.
        vuMeterIsHidden = mw->ui.audioMetreWidget->isHidden();
#ifdef _WIN32
        if (openglEnabled && vuMeterIsHidden)
            mw->ui.audioMetreWidget->setVisible(true);
#endif
    }

    if (!statusbarHidden)
        mw->addStatusBar();
    QuiMainWindows = (QWidget *)mw;

#ifdef _WIN32
    // On Windows, trying to open the main window maximized from the start
    // results in window state and window size going out of sync.
    // As a workaround, open non-maximized and maximize later in UI_RunApp().
    QuiMainWindows->show();
    if (!uiIsMaximized)
    {
        uint32_t chromeW = 0, chromeH = 0;
        mw->calcDockWidgetDimensions(chromeW, chromeH);
#if QT_VERSION < QT_VERSION_CHECK(5, 11, 0)
        QRect space = QApplication::desktop()->availableGeometry(QuiMainWindows);
#else
        QScreen *screen = QGuiApplication::screenAt(QuiMainWindows->frameGeometry().center());
        if (!screen)
            screen = QGuiApplication::primaryScreen();
        QRect space = screen->availableGeometry();
#endif
        int targetWidth = (std::max)(QuiMainWindows->width(), (int)chromeW + 720);
        int targetHeight = (std::max)(QuiMainWindows->height(), (int)chromeH + 640);
        if (space.isValid())
        {
            targetWidth = (std::min)(targetWidth, space.width());
            targetHeight = (std::min)(targetHeight, space.height());
        }
        if (targetWidth > QuiMainWindows->width() || targetHeight > QuiMainWindows->height())
            QuiMainWindows->resize(targetWidth, targetHeight);
        UI_centerMainWindowOnScreen();
    }
#else
    if (uiIsMaximized)
    {
        UI_setBlockZoomChangesFlag(false); // unblock zoom to fit
        QuiMainWindows->showMaximized();
    }
    else
    {
        QuiMainWindows->show();
    }
#endif

    uint32_t w, h;

    UI_getPhysicalScreenSize(QuiMainWindows, &w, &h);
    printf("The screen seems to be %u x %u px\n", w, h);
    mw->ui.frame_video->setAttribute(Qt::WA_OpaquePaintEvent);

    UI_QT4VideoWidget(mw->ui.frame_video);    // Add the widget that will handle video display
#if QT_VERSION < QT_VERSION_CHECK(5, 11, 0)   // not sure about the version
    mw->ui.frame_video->setAcceptDrops(true); // needed for drag and drop to work on windows
#endif
    admPreview::setMainDimension(0, 0, ZOOM_AUTO);

    UI_updateRecentMenu();
    UI_updateRecentProjectMenu();

    // Assign future parent of dummyGLWidget
    VuMeter =
#ifdef _WIN32
        mw->ui.frameVU;
#else
        mw->ui.frame_video;
#endif
    // Init VU meter
    UI_InitVUMeter(mw->ui.frameVU);

#ifdef USE_OPENGL
    if (openglEnabled)
    {
        ADM_info("OpenGL activated, initializing... \n");
        openGLStarted = true;
        UI_Qt4InitGl();
#ifdef _WIN32
        if (vuMeterIsHidden)
            mw->ui.audioMetreWidget->setVisible(false);
#endif
    }
    else
    {
        ADM_info("OpenGL not activated, not initialized\n");
    }
#endif
    mw->syncToolbarsMenu();

    return 1;
}
/**
 * \fn UI_closeGui
 */
void UI_closeGui(void)
{
    if (!uiRunning)
        return;
    uiRunning = false;

    QSettings *qset = qtSettingsCreate();
    if (qset)
    {
        qset->beginGroup("MainWindow");
        qset->setValue("windowState", ((QMainWindow *)QuiMainWindows)->saveState());
        qset->setValue("showMaximized", QuiMainWindows->isMaximized());
        qset->setValue("volume", WIDGET(horizontalSlider_2)->value());
        qset->setValue("statusbarHidden", !(((MainWindow *)QuiMainWindows)->statusBarEnabled()));
        qset->endGroup();
        delete qset;
        qset = NULL;
    }

    QuiMainWindows->close();
    qtUnregisterDialog(QuiMainWindows);
}

void destroyGUI(void)
{
}
void callBackQtWindowDestroyed()
{
}
/**

*/
bool UI_End(void)
{
    ADM_QPreviewCleanup();
    return true;
}
void UI_refreshCustomMenu(void)
{
    ((MainWindow *)QuiMainWindows)->buildCustomMenu();
}
/**
    \fn UI_applySettings
    \brief Do stuff when closing the preferences dialog
*/
void UI_applySettings(void)
{
    ((MainWindow *)QuiMainWindows)->updateActionShortcuts();
    ((MainWindow *)QuiMainWindows)->volumeWidgetOperational();
    ((MainWindow *)QuiMainWindows)->setRefreshCap();
}
/**
    \fn UI_getCurrentPreview
    \brief Read previewmode from checkable menu actions
*/
int UI_getCurrentPreview(void)
{
    QAction *a = findAction(&myMenuVideo, ACT_PreviewChanged);
    QAction *b = findActionInToolBar(WIDGET(toolBar), ACT_PreviewChanged);
    bool filtered = (a ? a->isChecked() : false) || (b ? b->isChecked() : false);

    if (filtered)
    {
        printf("Output is ON\n");
        return 1;
    }
    printf("Output is Off\n");
    return 0;
}

/**
    \fn UI_setCurrentPreview
    \brief Update "Play filtered" checkable menu actions with previewmode
*/
void UI_setCurrentPreview(int ne)
{
    QAction *preview = findAction(&myMenuVideo, ACT_PreviewChanged);
    if (preview)
        preview->setChecked(!!ne);
    preview = findActionInToolBar(WIDGET(toolBar), ACT_PreviewChanged);
    if (preview)
        preview->setChecked(!!ne);
}
/**
        \fn FatalFunctionQt
*/
extern void abortExitHandler();
static void FatalFunctionQt(const char *title, const char *info)
{
    printf("Crash Dump for %s\n", title);
    printf("%s\n", info);
    fflush(stdout);

    QMessageBox msgBox;
    msgBox.setText(title);
    msgBox.setInformativeText(QT_TRANSLATE_NOOP("qgui2", "The application has encountered a fatal problem\nThe current "
                                                         "editing has been saved and will be reloaded at next start"));
    msgBox.setDetailedText(info);
    msgBox.setIcon(QMessageBox::Critical);
    msgBox.exec();
    abortExitHandler(); // Try to cleanup
    abort();
}

/**
    \fn UI_RunApp(void)
    \brief Main entry point for the GUI application
*/
int UI_RunApp(void)
{
    uiRunning = true;
    setupMenus();
    QuiTaskBarProgress->setParent(QuiMainWindows);
    ADM_setCrashHook(&saveCrashProject, &FatalFunctionQt, &abortExitHandler);
#ifdef USE_DXVA2
    /* After d3d probing on startup, the host frame holding the video window
    cannot be shrunk to zero size unless the main window is resized, becoming
    visible as a white square if the main window is minimized and restored. */
    if (!uiIsMaximized)
    {
        int w = QuiMainWindows->width();
        int h = QuiMainWindows->height();
        QuiMainWindows->resize(w + 1, h);
        QuiMainWindows->resize(w, h);
    }
#endif
#ifdef _WIN32
    if (uiIsMaximized)
        QuiMainWindows->showMaximized();
#endif
    ADM_info("Load default settings if any... \n");
    A_loadDefaultSettings();
    UI_applySettings();

    // start update checking..
    bool autoUpdateEnabled = false;
    if (prefs->get(UPDATE_ENABLED, &autoUpdateEnabled))
    {
#ifndef _MSC_VER
        if (autoUpdateEnabled)
        {
            // Mark last check
            struct timeval tp;
            struct timezone tz;
            gettimeofday(&tp, &tz);
            uint32_t days = 1 + (tp.tv_sec - 1472894364) / (60 * 60 * 24); // days since 03 sept
            uint32_t lastCheck;
            prefs->get(UPDATE_LASTCHECK, &lastCheck);
            ADM_info("[autoUpdate]Current date %d , last check = %d\n", days, lastCheck);
            if (days > lastCheck)
            {
                prefs->set(UPDATE_LASTCHECK, days);
                prefs->save();
                ADM_checkForUpdate(&MainWindow::updateCheckDone);
            }
        }
#endif
    }

    myApplication->exec();
#ifdef USE_OPENGL
    if (openGLStarted)
    {
        ADM_info("OpenGL: Cleaning up\n");
        UI_Qt4CleanGl();
    }
#endif
    destroyTranslator();

    delete QuiMainWindows;
    delete myApplication;

    QuiMainWindows = NULL;
    myApplication = NULL;

    return 1;
}
/**
 * \fn updateCheckDone
 * @param version
 * @param date
 * @param downloadLink
 */
void MainWindow::updateCheckDone(int version, const std::string &date, const std::string &downloadLink)
{
    ADM_info("Version available %d from %s at %s\n", version, date.c_str(), downloadLink.c_str());
    emit mainWindowSingleton->updateAvailable(version, date, downloadLink);
}

/**
    \fn searchTranslationTable(const char *name))
    \brief return the action corresponding to a give button. The translation table is in translation_table.h
*/
Action searchTranslationTable(const char *name)
{
    for (int i = 0; i < SIZEOF_MY_TRANSLATION; i++)
    {
        if (!strcmp(name, myTranslationTable[i].name))
        {
            return myTranslationTable[i].action;
        }
    }
    printf("WARNING: Signal not found in translation table %s\n", name);
    return ACT_DUMMY;
}

/**
    \fn modifyTranslationTable(const char *name, Action a))
    \brief modify the action corresponding to a give button. The translation table is in translation_table.h
*/
bool modifyTranslationTable(const char *name, Action a)
{
    for (int i = 0; i < SIZEOF_MY_TRANSLATION; i++)
    {
        if (!strcmp(name, myTranslationTable[i].name))
        {
            myTranslationTable[i].action = a;
            return true;
        }
    }
    printf("WARNING: Signal not found in translation table %s\n", name);
    return false;
}

/**
    \fn     UI_updateRecentMenu( void )
    \brief  Update the recent submenu with the latest files loaded
*/
void UI_updateRecentMenu(void)
{
    ((MainWindow *)QuiMainWindows)->buildRecentMenu();
    ((MainWindow *)QuiMainWindows)->setMenuItemsEnabledState();
}

void UI_updateRecentProjectMenu()
{
    ((MainWindow *)QuiMainWindows)->buildRecentProjectMenu();
}

/**
  \fn    setupMenus(void)
  \brief Fill in video & audio co
*/
void setupMenus(void)
{
    uint32_t nbVid;
    uint32_t maj, mn, pa;
    const char *name;

    admLoadCustomOutputProfiles();
    admRefreshOutputProfileCombo(0);

    WIDGET(comboBoxProfileResizeMode)->clear();
    WIDGET(comboBoxProfileResizeMode)->addItem(admUiText("Original size", "原始尺寸", "原始尺寸"));
    WIDGET(comboBoxProfileResizeMode)->addItem(admUiText("Profile size, keep AR", "配置尺寸，保持宽高比", "設定尺寸，保持長寬比"));
    WIDGET(comboBoxProfileResizeMode)->addItem(admUiText("Custom size, keep AR", "自定义尺寸，保持宽高比", "自訂尺寸，保持長寬比"));
    WIDGET(comboBoxProfileResizeMode)->addItem(admUiText("Custom size, stretch", "自定义尺寸，拉伸", "自訂尺寸，拉伸"));
    admSetProfileResizeUi(0);

    nbVid = ADM_ve6_getNbEncoders();
    WIDGET(comboBoxVideo)->clear();
    printf("Found %d video encoder(s)\n", nbVid);
    for (uint32_t i = 0; i < nbVid; i++)
    {
        ADM_ve6_getEncoderInfo(i, &name, &maj, &mn, &pa);
        WIDGET(comboBoxVideo)->addItem(name);
    }

    // And A codec

    uint32_t nbAud;

    nbAud = audioEncoderGetNumberOfEncoders();
    printf("Found %d audio encoder(s)\n", nbAud);
    WIDGET(comboBoxAudio)->clear();
    for (uint32_t i = 0; i < nbAud; i++)
    {
        name = audioEncoderGetDisplayName(i);
        WIDGET(comboBoxAudio)->addItem(name);
    }

    /*   Fill in output format window */
    uint32_t nbFormat = ADM_mx_getNbMuxers();

    printf("Found %d format(s)\n", nbFormat);
    for (uint32_t i = 0; i < nbFormat; i++)
    {
        const char *name = ADM_mx_getDisplayName(i);
        WIDGET(comboBoxFormat)->addItem(name);
    }
    WIDGET(pushButtonFormatConfigure)->setEnabled((bool)nbFormat);
}
/*
    Return % of scale (between 0 and 1)
*/
double UI_readScale(void)
{
    double v;
    if (!slider)
        v = 0;
    v = (double)(slider->value());
    v /= ADM_SCALE_INCREMENT;
    return v;
}
void UI_setScale(double val)
{
    if (_upd_in_progres)
        return;
    _upd_in_progres++;
    bool old = slider->blockSignals(true);
    slider->setValue((int)(val * ADM_SCALE_INCREMENT));
    slider->blockSignals(old);
    _upd_in_progres--;
}

//*******************************************

/**
    \fn UI_setTitle(char *name)
    \brief Set the main window title, usually name if the file being edited
*/
void UI_setTitle(const char *name)
{
    char *title;
    const char *defaultTitle = "Avidemux";

    if (name && (*name))
    {
        title = new char[strlen(defaultTitle) + strlen(name) + 3 + 1];

        strcpy(title, name);
        strcat(title, " - ");
        strcat(title, defaultTitle);
    }
    else
    {
        title = new char[strlen(defaultTitle) + 1];

        strcpy(title, defaultTitle);
    }

    QuiMainWindows->setWindowTitle(QString::fromUtf8(title));
    delete[] title;
}

/**
    \fn     UI_setFrameType( uint32_t frametype,uint32_t qp)
    \brief  Display frametype (I/P/B) and associated quantizer
*/

void UI_setFrameType(uint32_t frametype, uint32_t qp)
{
    if (frametype == CLEAR_FRAME_TYPE)
    {
        WIDGET(label_8)->clear();
        return;
    }

    char string[100];
    char c = '?';
    const char *f = "???";
    switch (frametype & AVI_FRAME_TYPE_MASK)
    {
    case AVI_KEY_FRAME:
        c = 'I';
        break;
    case AVI_B_FRAME:
        c = 'B';
        break;
    case 0:
        c = 'P';
        break;
    default:
        break;
    }
    switch (frametype & AVI_STRUCTURE_TYPE_MASK)
    {
    case AVI_TOP_FIELD + AVI_FIELD_STRUCTURE:
        f = "TFF";
        break;
    case AVI_BOTTOM_FIELD + AVI_FIELD_STRUCTURE:
        f = "BFF";
        break;
    case AVI_FRAME_STRUCTURE:
        f = "FRM";
        break;
    default:
        f = "???";

        break;
    }
    if (qp == ADM_IMAGE_UNKNOWN_QP)
        sprintf(string, QT_TRANSLATE_NOOP("qgui2", "%c-%s"), c, f);
    else
        sprintf(string, QT_TRANSLATE_NOOP("qgui2", "%c-%s (%02d)"), c, f, qp);
    WIDGET(label_8)->setText(string);
}
/**
 *
 * @return
 */
admUITaskBarProgress *UI_getTaskBarProgress()
{
    return QuiTaskBarProgress;
}

/**
    \fn UI_setCurrentTime
    \brief Set current PTS of displayed video
*/
void UI_setCurrentTime(uint64_t curTime)
{
    char text[80];
    uint32_t mm, hh, ss, ms;
    uint32_t shorty = (uint32_t)(curTime / 1000);

    ms2time(shorty, &hh, &mm, &ss, &ms);
    sprintf(text, "%02d:%02d:%02d.%03d", hh, mm, ss, ms);
    WIDGET(currentTime)->setText(text);
}

/**
    \fn UI_setTotalTime
    \brief SEt the total duration of video
*/
void UI_setTotalTime(uint64_t curTime)
{
    char text[80];
    uint32_t mm, hh, ss, ms;
    uint32_t shorty = (uint32_t)(curTime / 1000);

    ms2time(shorty, &hh, &mm, &ss, &ms);
    sprintf(text, "/ %02d:%02d:%02d.%03d", hh, mm, ss, ms);
    WIDGET(totalTime)->setText(text);
    slider->setTotalDuration(curTime);
}
/**
    \fn UI_setSegments
    \brief SEt segments boundaries
*/
void UI_setSegments(uint32_t numOfSegs, uint64_t *segPts)
{
    slider->setSegments(numOfSegs, segPts);
}
/**
    \fn     UI_setMarkers(uint64_t Ptsa, uint32_t Ptsb )
    \brief  Display frame # for marker A & B
*/
void UI_setMarkers(uint64_t a, uint64_t b)
{
    char text[80];
    uint64_t absoluteA = a, absoluteB = b;
    uint32_t hh, mm, ss, ms;
    uint32_t timems;
    a /= 1000;
    b /= 1000;

    timems = (uint32_t)(a);
    ms2time(timems, &hh, &mm, &ss, &ms);
    snprintf(text, 79, "%02" PRIu32 ":%02" PRIu32 ":%02" PRIu32 ".%03" PRIu32, hh, mm, ss, ms);
    WIDGET(pushButtonJumpToMarkerA)->setText(text);

    timems = (uint32_t)(b);
    ms2time(timems, &hh, &mm, &ss, &ms);
    snprintf(text, 79, "%02" PRIu32 ":%02" PRIu32 ":%02" PRIu32 ".%03" PRIu32, hh, mm, ss, ms);
    WIDGET(pushButtonJumpToMarkerB)->setText(text);

    timems = (uint32_t)(b - a);
    ms2time(timems, &hh, &mm, &ss, &ms);
    snprintf(text, 79, "%02" PRIu32 ":%02" PRIu32 ":%02" PRIu32 ".%03" PRIu32, hh, mm, ss, ms);
    QString duration = QString::fromUtf8(QT_TRANSLATE_NOOP("qgui2", "Selection: ")) + QString(text);
    WIDGET(selectionDuration)->setText(duration);

    slider->setMarkers(absoluteA, absoluteB);
}

/**
    \fn     UI_getCurrentVCodec(void)
    \brief  Returns the current selected video code in menu, i.e its number (0 being the first)
*/
int UI_getCurrentVCodec(void)
{
    int i = WIDGET(comboBoxVideo)->currentIndex();
    if (i < 0)
        i = 0;
    return i;
}
/**
    \fn     UI_setVideoCodec( int i)
    \brief  Select the video codec which is # x in pulldown menu (starts at zero :copy)
*/

void UI_setVideoCodec(int i)
{
    int b = !!i;
    WIDGET(comboBoxVideo)->setCurrentIndex(i);

    WIDGET(pushButtonVideoConf)->setEnabled(b);
    WIDGET(pushButtonVideoFilter)->setEnabled(b);
}
/**
    \fn     UI_getCurrentACodec(void)
    \brief  Returns the current selected audio code in menu, i.e its number (0 being the first)
*/

int UI_getCurrentACodec(void)
{
    int i = WIDGET(comboBoxAudio)->currentIndex();
    if (i < 0)
        i = 0;
    return i;
}
/**
    \fn     UI_setAudioCodec( int i)
    \brief  Select the audio codec which is # x in pulldown menu (starts at zero :copy)
*/

void UI_setAudioCodec(int i)
{
    int b = !!i;
    WIDGET(comboBoxAudio)->setCurrentIndex(i);
    WIDGET(pushButtonAudioConf)->setEnabled(b);
    WIDGET(pushButtonAudioFilter)->setEnabled(b);
}
/**
    \fn     UI_GetCurrentFormat(void)
    \brief  Returns the current selected output format
*/

int UI_GetCurrentFormat(void)
{
    int i = WIDGET(comboBoxFormat)->currentIndex();
    if (i < 0)
        i = 0;
    return (int)i;
}
/**
    \fn     UI_SetCurrentFormat( ADM_OUT_FORMAT fmt )
    \brief  Select  output format
*/
void UI_SetCurrentFormat(uint32_t fmt)
{
    WIDGET(comboBoxFormat)->setCurrentIndex((int)fmt);
}

/**
      \fn UI_getTimeShift
      \brief get state (on/off) and value for time Shift
*/
bool UI_getTimeShift(int *onoff, int *value)
{
    if (WIDGET(checkBox_TimeShift)->checkState() == Qt::Checked)
        *onoff = 1;
    else
        *onoff = 0;
    *value = WIDGET(spinBox_TimeValue)->value();
    return 1;
}
/**
      \fn UI_setTimeShift
      \brief get state (on/off) and value for time Shift
*/

bool UI_setTimeShift(int onoff, int value)
{
    if (onoff && value)
        WIDGET(checkBox_TimeShift)->setCheckState(Qt::Checked);
    else
        WIDGET(checkBox_TimeShift)->setCheckState(Qt::Unchecked);
    WIDGET(spinBox_TimeValue)->setValue(value);
    return 1;
}
/**
    \fn UI_setVUMeter
*/
bool UI_setVUMeter(int32_t volume[8])
{
    UI_vuUpdate(volume);
    return true;
}

/**
    \fn UI_setVolume
*/
bool UI_setVolume(void)
{
    if (WIDGET(toolButtonAudioToggle)->isChecked())
        ((MainWindow *)QuiMainWindows)->volumeChange(0);
    else
        AVDM_setVolume(0);
    return true;
}

/**
    \fn UI_setDecoderName
*/
bool UI_setDecoderName(const char *name)
{
    ((MainWindow *)QuiMainWindows)->updateStatusBarDecoderInfo(name);
    return true;
}
/**
 * \fn UI_setDisplayName
 * \brief display current displayEngine name
 */
bool UI_setDisplayName(const char *name)
{
    ((MainWindow *)QuiMainWindows)->updateStatusBarDisplayInfo(name);
    return true;
}

/**
    \fn UI_navigationButtonsPressed
    \brief Allow to abstain from opening pop-up dialogs while
           a button with auto-repeat enabled is pressed, else
           the mouse release event gets eaten by the pop-up and
           we keep firing the action assigned to the particular
           button forever.
*/
bool UI_navigationButtonsPressed(void)
{
    if (WIDGET(toolButtonPreviousFrame)->isDown())
        return true;
    if (WIDGET(toolButtonNextFrame)->isDown())
        return true;
    if (WIDGET(toolButtonPreviousIntraFrame)->isDown())
        return true;
    if (WIDGET(toolButtonNextIntraFrame)->isDown())
        return true;
    return false;
}

/**
    \fn UI_hasOpengl
*/
bool UI_hasOpenGl(void)
{
#ifndef USE_OPENGL
    return false;
#else
    if (!ADM_glHasActiveTexture())
        return false; // ADM_setActiveTexure
    bool enabled;
    prefs->get(FEATURES_ENABLE_OPENGL, &enabled);
    return enabled;
#endif
}
/**
    \fn UI_iconify
*/
void UI_iconify(void)
{
    uiIsMaximized = QuiMainWindows->isMaximized();
    QuiMainWindows->hide();
}
/**
    \fn UI_deiconify
*/
void UI_deiconify(void)
{
    if (uiIsMaximized)
    {
        QuiMainWindows->showMaximized();
    }
    else
    {
        QuiMainWindows->showNormal();
    }
}
/**
 *    \fn UI_resize
 *    \brief resize the main window for the given dimensions of the video widget
 */
void UI_resize(uint32_t w, uint32_t h)
{
    if (((MainWindow *)QuiMainWindows)->getBlockResizingFlag())
        return;
    uint32_t reqw, reqh;
    ((MainWindow *)QuiMainWindows)->calcDockWidgetDimensions(reqw, reqh);
    reqw += w;
    reqh += h;
    uint32_t screenW = 0, screenH = 0;
#if QT_VERSION < QT_VERSION_CHECK(5, 11, 0)
    QRect space = QApplication::desktop()->availableGeometry();
#else
    QRect space = QGuiApplication::primaryScreen()->availableGeometry();
#endif

    if (reqw > (uint32_t)space.width())
        reqw = space.width();
    if (reqh > (uint32_t)space.height())
        reqh = space.height();

    UI_setBlockZoomChangesFlag(true);
    QuiMainWindows->resize(reqw, reqh);
    ADM_info("Resizing the main window to %dx%d px (Screen: %dx%d)\n", reqw, reqh, space.width(), space.height());
#ifdef _WIN32
    UI_centerMainWindowOnScreen();
#endif
    UI_setBlockZoomChangesFlag(false);
    ((MainWindow *)QuiMainWindows)->setResizeThreshold(RESIZE_THRESHOLD);
}

/**
    \fn UI_getMaximumPreviewSize
    \brief Return maximum width and height available for video preview
*/
void UI_getMaximumPreviewSize(uint32_t *availWidth, uint32_t *availHeight)
{
    *availWidth = 1;
    *availHeight = 1;

    if (QuiMainWindows)
    {
        MainWindow *mw = (MainWindow *)QuiMainWindows;
        QRect videoArea = mw->ui.frame_video->contentsRect();
        int videoWidth = videoArea.width();
        int videoHeight = videoArea.height();
        if (videoWidth > 0 && videoHeight > 0)
        {
            *availWidth = (uint32_t)videoWidth;
            *availHeight = (uint32_t)videoHeight;
            return;
        }

        uint32_t reqw, reqh;
        mw->calcDockWidgetDimensions(reqw, reqh);
        int fallbackWidth = QuiMainWindows->width() - (int)reqw;
        int fallbackHeight = QuiMainWindows->height() - (int)reqh;
        if (fallbackWidth > 0)
            *availWidth = (uint32_t)fallbackWidth;
        if (fallbackHeight > 0)
            *availHeight = (uint32_t)fallbackHeight;
    }
}

/**
    \fn UI_getNeedsResizingFlag
*/
bool UI_getNeedsResizingFlag(void)
{
    return needsResizing;
}

/**
    \fn UI_setNeedsResizingFlag
*/
void UI_setNeedsResizingFlag(bool resize)
{
    needsResizing = resize;
}

/**
    \fn UI_setBlockZoomChangesFlag
*/
void UI_setBlockZoomChangesFlag(bool block)
{
    ((MainWindow *)QuiMainWindows)->setBlockZoomChangesFlag(block);
}

/**
    \fn UI_resetZoomThreshold
*/
void UI_resetZoomThreshold(void)
{
    ((MainWindow *)QuiMainWindows)->setResizeThreshold(RESIZE_THRESHOLD);
    ((MainWindow *)QuiMainWindows)->setActZoomCalledFlag(true);
}

/**
    \fn UI_setZoomToFitIntoWindow
*/
void UI_setZoomToFitIntoWindow(void)
{
    ((MainWindow *)QuiMainWindows)->setZoomToFit();
}

/**
    \fn UI_displayZoomLevel
*/
void UI_displayZoomLevel(void)
{
    ((MainWindow *)QuiMainWindows)->updateZoomIndicator();
}

/**
    \fn UI_setAudioTrackCount
*/
void UI_setAudioTrackCount(int nb)
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
    QString text = QCoreApplication::translate("qgui2", " (%n track(s))", NULL, nb);
#else
    QString text = QCoreApplication::translate("qgui2", " (%n track(s))", NULL, QCoreApplication::UnicodeUTF8, nb);
#endif
    WIDGET(TrackCountLabel)->setText(text);
}

/**
    \fn UI_notifyInfo
*/
void UI_notifyInfo(const char *message, int timeoutMs)
{
    ((MainWindow *)QuiMainWindows)->notifyStatusBar(0, QT_TRANSLATE_NOOP("qgui2", "INFO: %1"), message, timeoutMs);
}
/**
    \fn UI_notifyWarning
*/
void UI_notifyWarning(const char *message, int timeoutMs)
{
    ((MainWindow *)QuiMainWindows)->notifyStatusBar(1, QT_TRANSLATE_NOOP("qgui2", "WARNING: %1"), message, timeoutMs);
}
/**
    \fn UI_notifyError
*/
void UI_notifyError(const char *message, int timeoutMs)
{
    ((MainWindow *)QuiMainWindows)->notifyStatusBar(2, QT_TRANSLATE_NOOP("qgui2", "ERROR: %1"), message, timeoutMs);
}
/**
    \fn UI_notifyPlaybackLag
*/
void UI_notifyPlaybackLag(uint32_t lag, int updateTimeMs)
{
    char value[128];
    sprintf(value, "%u", lag);
    if (updateTimeMs < 500)
        updateTimeMs = 500;
    ((MainWindow *)QuiMainWindows)
        ->notifyStatusBar(1, QT_TRANSLATE_NOOP("qgui2", "WARNING: Video is late by %1 ms"), value, updateTimeMs, false);
}
/**
    \fn UI_tweaks
*/
void UI_tweaks(const char *op, const char *paramS, int paramN)
{
    if (strcmp("STATUSBAR_SHOW_INFO", op) == 0)
    {
        UI_notifyInfo(paramS, paramN);
    }
    else if (strcmp("STATUSBAR_SHOW_WARNING", op) == 0)
    {
        UI_notifyWarning(paramS, paramN);
    }
    else if (strcmp("STATUSBAR_SHOW_ERROR", op) == 0)
    {
        UI_notifyError(paramS, paramN);
    }
    else if (strcmp("CURSOR_SET_BUSY", op) == 0)
    {
        if (paramN)
            QApplication::setOverrideCursor(Qt::WaitCursor);
        else
            QApplication::restoreOverrideCursor();
    }
    else if (strcmp("WINDOW_MINIMIZE", op) == 0)
    {
        // UI_iconify(); <-- taskbar icon disappears
        uiIsMaximized = QuiMainWindows->isMaximized();
        QuiMainWindows->setWindowState(Qt::WindowMinimized);
    }
    else if (strcmp("WINDOW_RESTORE", op) == 0)
    {
        UI_deiconify();
    }
    else if (strcmp("WINDOW_SET_TITLE", op) == 0)
    {
        UI_setTitle(paramS);
    }
    else if (strcmp("GUI_EXIT", op) == 0)
    {
        if (qtLastRegisteredDialog() == QuiMainWindows)
        {
            UI_closeGui();
        }
        else
        {
            ADM_error("Exit is only possible, when there are no dialogs open");
        }
    }
    else if (strcmp("SET_PLAY_FILTERED", op) == 0)
    {
        UI_setCurrentPreview(paramN);
    }
    else if (strcmp("SET_LIGT_THEME", op) == 0)
    {
        ((MainWindow *)QuiMainWindows)->setLightTheme();
    }
    else if (strcmp("SET_DARK_THEME", op) == 0)
    {
        ((MainWindow *)QuiMainWindows)->setDarkTheme();
    }
    else if (strcmp("TEXT_TO_CLIPBOARD", op) == 0)
    {
        QClipboard *clipboard = QApplication::clipboard();
        clipboard->clear();
        clipboard->setText(paramS);
    }
    else

    {
        // NOP
    }
    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
}

/**
 * \fn dtor
 */
myQApplication::~myQApplication()
{
    ADM_clearQtShellHistory();
    ADM_warning("Cleaning render...\n");
    renderDestroy();
    ADM_warning("Cleaning preview...\n");
    admPreview::cleanUp();
    ADM_ExitCleanup();

#if defined(USE_VDPAU)
#if (ADM_UI_TYPE_BUILD != ADM_UI_CLI)
    ADM_warning("cleaning VDPAU...\n");
    vdpauCleanup();
#else
    ADM_info("Cannot use VDPAU in cli mode %d,%d\n", ADM_UI_TYPE_BUILD, ADM_UI_CLI);
#endif
#endif

#if 0
#ifdef SDL_ON_LINUX
    ADM_warning("This is SDL on linux, exiting brutally to avoid lock.\n");
    ::exit(0); //
#endif
#endif
    ADM_warning("Exiting app\n");
}

//********************************************
// EOF
