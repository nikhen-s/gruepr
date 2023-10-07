#ifndef GETGRUEPRDATADIALOG_H
#define GETGRUEPRDATADIALOG_H

#include <QDialog>
#include "canvashandler.h"
#include "csvfile.h"
#include "dataOptions.h"
#include "googlehandler.h"
#include "studentRecord.h"

namespace Ui {
class GetGrueprDataDialog;
}

class GetGrueprDataDialog : public QDialog
{
    Q_OBJECT

public:
    explicit GetGrueprDataDialog(QWidget *parent = nullptr);
    ~GetGrueprDataDialog();

    DataOptions *dataOptions = nullptr;
    QList<StudentRecord> students;

private:
    Ui::GetGrueprDataDialog *ui;

    DataOptions::DataSource source = DataOptions::fromFile;

    QList<StudentRecord> roster;    // holds roster of students from alternative source (in order to add names of non-submitters)

    void loadData();
    CsvFile *surveyFile = nullptr;
    bool getFromFile();
    bool getFromGoogle();
    bool getFromCanvas();
    bool getFromPrevWork();
    bool readQuestionsFromHeader();
    void validateFieldSelectorBoxes(int callingRow = -1);
    bool readData();
    CanvasHandler *canvas = nullptr;
    GoogleHandler *google = nullptr;
    const QString HEADERTEXT = tr("Question text");
    const QString CATEGORYTEXT = tr("Category");
    const QString ROW1TEXT = tr("First Row of Data");
    const QString UNUSEDTEXT = tr("Unused");
};

#endif // GETGRUEPRDATADIALOG_H
