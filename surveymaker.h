#ifndef SURVEYMAKER_H
#define SURVEYMAKER_H

#include "dialogs/dayNamesDialog.h"
#include "gruepr_consts.h"
#include "survey.h"
#include "widgets/comboBoxWithElidedContents.h"
#include <QDate>
#include <QFileInfo>
#include <QLabel>
#include <QMainWindow>
#include <QRegularExpressionValidator>

namespace Ui {class SurveyMaker;}

class SurveyMaker : public QMainWindow
{
    Q_OBJECT

public:
    explicit SurveyMaker(QWidget *parent = nullptr);
    ~SurveyMaker();

protected:
    void resizeEvent(QResizeEvent *event);
    void closeEvent(QCloseEvent *event);

private slots:
    void on_surveyTitleLineEdit_textChanged(const QString &arg1);
    void on_attributeCountSpinBox_valueChanged(int arg1);
    void attributeTabBarMoveTab(int indexFrom, int indexTo);
    void refreshAttributeTabBar(int index);
    void attributeTabClose(int index);
    void attributeTextChanged();
    void on_timezoneCheckBox_clicked(bool checked);
    void on_scheduleCheckBox_clicked(bool checked);
    void checkTimezoneAndSchedule();
    void baseTimezoneComboBox_currentIndexChanged(int arg1);
    void on_baseTimezoneLineEdit_textChanged();
    void on_daysComboBox_activated(int index);
    void day_CheckBox_toggled(bool checked, QLineEdit *dayLineEdit, const QString &dayname);
    void day_LineEdit_textChanged(const QString &text, QLineEdit *dayLineEdit, QString &dayname);
    void on_timeStartEdit_timeChanged(QTime time);
    void on_timeEndEdit_timeChanged(QTime time);
    void on_sectionCheckBox_clicked(bool checked);
    void on_sectionNamesTextEdit_textChanged();
    void on_makeSurveyButton_clicked();
    void on_surveyDestinationBox_currentIndexChanged(const QString &arg1);
    void openSurvey();
    void saveSurvey();
    void settingsWindow();
    void helpWindow();
    void aboutWindow();

private:
    Ui::SurveyMaker *ui;
    Survey *survey;
    void refreshPreview();
    void createQuestion(QString &previewText, const QString &questionText, const Question::QuestionType questionType, const QString &options = "");
    void checkDays();
    bool surveyCreated = false;
    QRegularExpressionValidator *noInvalidPunctuation;
    void badExpression(QWidget *textWidget, QString &currText);
    bool firstname = true;
    bool lastname = true;
    bool email = true;
    bool gender = true;
    bool URM = false;
    int numAttributes = 3;
    QString attributeTexts[MAX_ATTRIBUTES] = {""};
    int attributeResponses[MAX_ATTRIBUTES] = {0};
    bool attributeAllowMultipleResponses[MAX_ATTRIBUTES] = {false};
    bool schedule = true;
    enum {busy, free} busyOrFree = free;
    bool timezone = false;
    QString baseTimezone = "";
    ComboBoxWithElidedContents *baseTimezoneComboBox = nullptr;
    enum TimezoneType {noneOrHome, custom=2, set=4};
    QStringList defaultDayNames;
    QString dayNames[MAX_DAYS];
    inline static const QDate sunday = QDate(2017, 1, 1);
    QLineEdit *dayLineEdits[MAX_DAYS] = {nullptr};
    QCheckBox *dayCheckBoxes[MAX_DAYS] = {nullptr};
    dayNamesDialog *daysWindow = nullptr;
    int startTime = 10;
    int endTime = 17;
    bool section = false;
    QStringList sectionNames = {""};
    bool preferredTeammates = false;
    bool preferredNonTeammates = false;
    int numPreferredAllowed = 1;
    bool additionalQuestions = false;
    static void postGoogleURL(SurveyMaker *survey = nullptr);
    static void createFiles(SurveyMaker *survey = nullptr);
    void (*generateSurvey)(SurveyMaker *survey) = SurveyMaker::postGoogleURL;
    QFileInfo saveFileLocation;
    QStringList responseOptions;
    enum {Sun, Mon, Tue, Wed, Thu, Fri, Sat};
    inline static const int LAST_LIKERT_RESPONSE = 25;
    inline static const int TIMEZONE_RESPONSE_OPTION = 101;
    inline static const QSize TABCLOSEICONSIZE = {8,8};
    inline static const QString QUESTIONPREVIEWHEAD = "<p>&nbsp;&nbsp;&nbsp;&bull;&nbsp;";
    inline static const QString QUESTIONPREVIEWTAIL = "<br></p>";
    inline static const QString QUESTIONOPTIONSHEAD = "<br>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<small>" + QObject::tr("options") + ": <b>{</b><i>";
    inline static const QString QUESTIONOPTIONSTAIL = "</i><b>}</b></small>";
    inline static const QString FIRSTNAMEQUESTION = QObject::tr("What is your first name (or the name you prefer to be called)?");
    inline static const QString LASTNAMEQUESTION = QObject::tr("What is your last name?");
    inline static const QString EMAILQUESTION = QObject::tr("What is your email address?");
    inline static const QString GENDERQUESTION = QObject::tr("With which gender do you identify most closely?");
    inline static const QString GENDEROPTIONS = QObject::tr("woman/man/nonbinary/prefer not to answer");
    inline static const QString URMQUESTION = QObject::tr("How do you identify your race, ethnicity, or cultural heritage?");
    inline static const QString TIMEZONEQUESTION = QObject::tr("What time zone will you be based in during this class?");
    inline static const QString TIMEZONEOPTIONS = QObject::tr("dropdown box of world timezones");
    inline static const QString SCHEDULEQUESTION1 = QObject::tr("Check the times that you are ");
    inline static const QString SCHEDULEQUESTION2BUSY = QObject::tr("BUSY and will be UNAVAILABLE");
    inline static const QString SCHEDULEQUESTION2FREE = QObject::tr("FREE and will be AVAILABLE");
    inline static const QString SCHEDULEQUESTION3 = QObject::tr(" for group work.");
    inline static const QString SCHEDULEQUESTION4 = QObject::tr(" These times refer to <u><strong>");
    inline static const QString SCHEDULEQUESTIONHOME = QObject::tr("your home");
    inline static const QString SCHEDULEQUESTION5 = QObject::tr("</strong></u> timezone.");
    inline static const QString SECTIONQUESTION = QObject::tr("In which section are you enrolled?");
    inline static const QString PREF1TEAMMATEQUESTION = QObject::tr("Please write the name of someone who you would like to have on your team. Write their first and last name only.");
    inline static const QString PREF1NONTEAMMATEQUESTION = QObject::tr("Please write the name of someone who you would like to NOT have on your team. Write their first and last name only.");
    inline static const QString PREFMULTQUESTION1 = QObject::tr("Please list the name(s) of up to ");
    inline static const QString PREFMULTQUESTION2YES = QObject::tr(" people who you would like to have on your team. Write their first and last name, and put a comma between multiple names.");
    inline static const QString PREFMULTQUESTION2NO = QObject::tr(" people who you would like to NOT have on your team. Write their first and last name, and put a comma between multiple names.");
    inline static const QString ADDLQUESTION = QObject::tr("Any additional things we should know about you before we form the teams?");
};

#endif // SURVEYMAKER_H
