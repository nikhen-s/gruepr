#ifndef GOOGLEHANDLER_H
#define GOOGLEHANDLER_H

#include "LMS.h"
#include "survey.h"

struct GoogleForm
{
    QString name;
    QString ID;
    QString createdTime;
    QUrl responderURL;
};

struct GoogleFormQuestion
{
    QString ID;
    QString text;
    enum class Type{notSchedule, schedule} type = Type::notSchedule;
};

class GoogleHandler : public LMS
{
    Q_OBJECT

public:
    GoogleHandler(QObject *parent = nullptr);
    ~GoogleHandler() override;
    GoogleHandler(const GoogleHandler&) = delete;
    GoogleHandler operator= (const GoogleHandler&) = delete;
    GoogleHandler(GoogleHandler&&) = delete;
    GoogleHandler& operator= (GoogleHandler&&) = delete;

    bool authenticate() override;
    GoogleForm createSurvey(const Survey *const survey);
    QStringList getSurveyList();
    QString downloadSurveyResult(const QString &surveyName);

    static QPixmap icon();

private:
    void postToGoogleGetSingleResult(const QString &URL, const QByteArray &postData, const QStringList &stringParams, QList<QStringList*> &stringVals,
                                                                                     const QStringList &stringInSubobjectParams, QList<QStringList*> &stringInSubobjectVals);

    QList<GoogleForm> formsList;

    QString getScopes() const override;
    QString getClientID() const override;
    QString getClientSecret() const override;
    QString getActionDialogIcon() const override;
    QString getActionDialogLabel() const override;

    inline static const char AUTHENTICATEURL[]{"https://accounts.google.com/o/oauth2/auth"};
    inline static const char ACCESSTOKENURL[]{"https://oauth2.googleapis.com/token"};
    //inline static const char CLIENT_ID[]{** Hidden in googlesecrets.h **};
    //inline static const char CLIENT_SECRET[]{** Hidden in googlesecrets.h **};
};

#endif // GOOGLEHANDLER_H
