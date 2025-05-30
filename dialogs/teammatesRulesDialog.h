#ifndef TEAMMATESRULESDIALOG_H
#define TEAMMATESRULESDIALOG_H

#include <QDialog>
#include "dataOptions.h"
#include "qabstractbutton.h"
#include "qboxlayout.h"
#include "qtablewidget.h"
#include "studentRecord.h"
#include "teamingOptions.h"
#include <QComboBox>

namespace Ui {
class TeammatesRulesDialog;
}

class TeammatesRulesDialog : public QDialog
{
    Q_OBJECT

public:
    enum class TypeOfTeammates{required, prevented, requested};
    explicit TeammatesRulesDialog(const QList<StudentRecord> &incomingStudents, const DataOptions &dataOptions, const TeamingOptions &teamingOptions,
                                  const QString &sectionname, const QStringList &currTeamSets, QWidget *parent = nullptr,
                                  bool autoLoadRequired = false, bool autoLoadPrevented = false, bool autoLoadRequested = false, int initialTabIndex = 0);
    ~TeammatesRulesDialog() override;
    TeammatesRulesDialog(const TeammatesRulesDialog&) = delete;
    TeammatesRulesDialog operator= (const TeammatesRulesDialog&) = delete;
    TeammatesRulesDialog(TeammatesRulesDialog&&) = delete;
    TeammatesRulesDialog& operator= (TeammatesRulesDialog&&) = delete;

    QHBoxLayout *headerLayoutRequiredTeammates;
    QHBoxLayout *headerLayoutPreventedTeammates;
    QHBoxLayout *headerLayoutRequestedTeammates;
    QWidget *headerRequiredTeammates;
    QWidget *headerPreventedTeammates;
    QWidget *headerRequestedTeammates;
    QAbstractButton *topLeftTableHeaderButton;
    int initialWidthStudentHeader;

    QList<StudentRecord> students;
    bool required_teammatesSpecified = false;
    bool prevented_teammatesSpecified = false;
    bool requested_teammatesSpecified = false;
    int numberRequestedTeammatesGiven = 1;
    QTableWidget* required_tableWidget;
    QTableWidget* prevented_tableWidget;
    QTableWidget* requested_tableWidget;

private slots:
    void clearAllValues();

private:
    Ui::TeammatesRulesDialog *ui;
    QPushButton *clearAllValuesButton = nullptr;

    bool positiverequestsInSurvey = false;
    bool negativerequestsInSurvey = false;
    const int numStudents;
    QString sectionName;
    QStringList teamSets;

    QList <QComboBox *> possibleRequiredTeammates;
    QList <QComboBox *> possiblePreventedTeammates;
    QList <QComboBox *> possibleRequestedTeammates;

    void showToast(QWidget *parent, const QString &message, int duration = 3000);
    void initializeTableHeaders(TypeOfTeammates typeOfTeammates, QString searchBarText = "", bool initializeStatus = false);
    void addTeammateSelector(TypeOfTeammates typeOfTeammates);
    void refreshDisplay(TypeOfTeammates typeOfTeammates, int verticalScrollPos, int horizontalScrollPos, QString searchBarText="");
    void addOneTeammateSet(TypeOfTeammates typeOfTeammates);
    void clearValues(TypeOfTeammates typeOfTeammates, bool verify = true);

    // these all return true on success, false on fail
    //bool saveCSVFile(TypeOfTeammates typeOfTeammates);    // removed the save button once autosaving implemented
    bool loadCSVFile(TypeOfTeammates typeOfTeammates);
    bool loadStudentPrefs(TypeOfTeammates typeOfTeammates);
    bool loadSpreadsheetFile(TypeOfTeammates typeOfTeammates);
    bool loadExistingTeamset(TypeOfTeammates typeOfTeammates);

    const QSize ICONSIZE = QSize(15,15);
};

#endif // TEAMMATESRULESDIALOG_H
