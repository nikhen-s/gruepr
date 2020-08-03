#include "ui_gruepr.h"
#include "gruepr.h"
#include <QFile>
#include <QFileDialog>
#include <QJsonDocument>
#include <QList>
#include <QMessageBox>
#include <QPainter>
#include <QPrintDialog>
#include <QScreen>
#include <QTextBrowser>
#include <QTextStream>
#include <QtConcurrent>
#include <QtNetwork>
#include <QDesktopServices>
#include <random>
#include <set>


gruepr::gruepr(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::gruepr)
{
    //Setup the main window
    ui->setupUi(this);
    setWindowFlags(Qt::Window | Qt::WindowTitleHint | Qt::WindowCloseButtonHint | Qt::WindowMinMaxButtonsHint);
    setWindowIcon(QIcon(":/icons/gruepr.png"));
    qRegisterMetaType<QVector<float> >("QVector<float>");

    //Setup the main window menu items
    connect(ui->actionLoad_Survey_File, &QAction::triggered, this, &gruepr::on_loadSurveyFileButton_clicked);
    connect(ui->actionSave_Survey_File, &QAction::triggered, this, &gruepr::on_saveSurveyFilePushButton_clicked);
    connect(ui->actionLoad_Teaming_Options_File, &QAction::triggered, this, &gruepr::loadOptionsFile);
    connect(ui->actionSave_Teaming_Options_File, &QAction::triggered, this, &gruepr::saveOptionsFile);
    connect(ui->actionCreate_Teams, &QAction::triggered, this, &gruepr::on_letsDoItButton_clicked);
    connect(ui->actionSave_Teams, &QAction::triggered, this, &gruepr::on_saveTeamsButton_clicked);
    connect(ui->actionPrint_Teams, &QAction::triggered, this, &gruepr::on_printTeamsButton_clicked);
    ui->actionExit->setMenuRole(QAction::QuitRole);
    connect(ui->actionExit, &QAction::triggered, this, &gruepr::close);
    //ui->actionSettings->setMenuRole(QAction::PreferencesRole);
    //connect(ui->actionSettings, &QAction::triggered, this, &gruepr::settingsWindow);
    connect(ui->actionHelp, &QAction::triggered, this, &gruepr::helpWindow);
    ui->actionAbout->setMenuRole(QAction::AboutRole);
    connect(ui->actionAbout, &QAction::triggered, this, &gruepr::aboutWindow);
    connect(ui->actiongruepr_Homepage, &QAction::triggered, this, [] {QDesktopServices::openUrl(QUrl("https://bit.ly/grueprFromApp"));});

    //Set alternate fonts on some UI features
    QFont altFont = this->font();
    altFont.setPointSize(altFont.pointSize() + 4);
    ui->loadSurveyFileButton->setFont(altFont);
    ui->letsDoItButton->setFont(altFont);
    ui->addStudentPushButton->setFont(altFont);
    ui->saveSurveyFilePushButton->setFont(altFont);
    ui->saveTeamsButton->setFont(altFont);
    ui->printTeamsButton->setFont(altFont);
    ui->dataDisplayTabWidget->setFont(altFont);
    ui->teamingOptionsGroupBox->setFont(altFont);

    //Reduce size of the options icons if the screen is small
#ifdef Q_OS_WIN32
    if(QGuiApplication::primaryScreen()->availableSize().height() < 900)
#endif
#ifdef Q_OS_MACOS
    if(QGuiApplication::primaryScreen()->availableSize().height() < 800)
#endif
    {
        ui->label_15->setMaximumSize(30,30);
        ui->label_16->setMaximumSize(30,30);
        ui->label_18->setMaximumSize(30,30);
        ui->label_20->setMaximumSize(30,30);
        ui->label_21->setMaximumSize(30,30);
        ui->label_22->setMaximumSize(30,30);
        ui->label_24->setMaximumSize(30,30);
    }
    adjustSize();

    //Disallow sorting on last two columns of student table
    connect(ui->studentTable->horizontalHeader(), &QHeaderView::sectionClicked, this, [this](int column)
                                                                                {if(column < ui->studentTable->columnCount()-2)
                                                                                      {ui->studentTable->sortByColumn(column, ui->studentTable->horizontalHeader()->sortIndicatorOrder());
                                                                                       ui->studentTable->horizontalHeaderItem(column)->setIcon(QIcon(":/icons/blank_arrow.png"));
                                                                                       if(column != prevSortColumn)
                                                                                       {ui->studentTable->horizontalHeaderItem(prevSortColumn)->setIcon(QIcon(":/icons/updown_arrow.png"));}
                                                                                       prevSortColumn = column;
                                                                                       prevSortOrder = ui->studentTable->horizontalHeader()->sortIndicatorOrder();}
                                                                                 else
                                                                                      {ui->studentTable->sortByColumn(prevSortColumn, prevSortOrder);
                                                                                       ui->studentTable->horizontalHeader()->setSortIndicator(prevSortColumn, prevSortOrder);}});

    //Add the teamDataTree widget
    teamDataTree = new TeamTreeWidget(this);
    teamDataTree->setGeometry(0,0,624,626);
    ui->teamDataLayout->insertWidget(0, teamDataTree);
    connect(teamDataTree, &TeamTreeWidget::swapChildren, this, &gruepr::swapTeammates);
    connect(teamDataTree, &TeamTreeWidget::swapParents, this, &gruepr::swapTeams);
    connect(teamDataTree, &TeamTreeWidget::updateTeamOrder, this, &gruepr::reorderedTeams);

    //Initialize remaining parameters
    for(int attrib = 0; attrib < maxAttributes; attrib++)
    {
        realAttributeWeights[attrib] = 1;
        haveAnyIncompatibleAttributes[attrib] = false;
    }

    //Load team name options into combo box
    ui->teamNamesComboBox->insertItems(0, QString(teamNameNames).split(","));

    //Connect genetic algorithm progress signals to slots
    connect(this, &gruepr::generationComplete, this, &gruepr::updateOptimizationProgress);
    connect(&futureWatcher, &QFutureWatcher<void>::finished, this, &gruepr::optimizationComplete);

    // load all of the default values
    loadDefaultSettings();
}

gruepr::~gruepr()
{
    delete teamDataTree;
    delete[] student;
    delete[] expanded;
    delete ui;
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////
// Slots
/////////////////////////////////////////////////////////////////////////////////////////////////////////


void gruepr::on_loadSurveyFileButton_clicked()
{
    QString fileName = QFileDialog::getOpenFileName(this, tr("Open Survey Data File"), dataOptions.dataFile.canonicalPath(), tr("Survey Data File (*.csv *.txt);;All Files (*)"));

    if (!fileName.isEmpty())
    {
        QApplication::setOverrideCursor(QCursor(Qt::BusyCursor));

        // Reset the various UI components
        ui->minMeetingTimes->setEnabled(false);
        ui->desiredMeetingTimes->setEnabled(false);
        ui->meetingLength->setEnabled(false);
        ui->scheduleWeight->setEnabled(false);
        ui->label_16->setEnabled(false);
        ui->label_0->setEnabled(false);
        ui->label_6->setEnabled(false);
        ui->label_7->setEnabled(false);
        ui->label_8->setEnabled(false);
        ui->label_9->setEnabled(false);
        ui->requiredTeammatesButton->setEnabled(false);
        ui->label_18->setEnabled(false);
        ui->preventedTeammatesButton->setEnabled(false);
        ui->requestedTeammatesButton->setEnabled(false);
        ui->label_11->setEnabled(false);
        ui->requestedTeammateNumberBox->setEnabled(false);
        ui->sectionSelectionBox->clear();
        ui->sectionSelectionBox->setEnabled(false);
        ui->label_2->setEnabled(false);
        ui->label_22->setEnabled(false);
        ui->attributeLabel->clear();
        ui->attributeLabel->setEnabled(false);
        ui->attributeScrollBar->setEnabled(false);
        ui->attributeTextEdit->clear();
        ui->attributeTextEdit->setEnabled(false);
        ui->attributeWeight->setEnabled(false);
        ui->attributeHomogeneousBox->setEnabled(false);
        ui->incompatibleResponsesButton->setEnabled(false);
        ui->label_21->setEnabled(false);
        ui->label_5->setEnabled(false);
        ui->studentTable->clear();
        ui->studentTable->setRowCount(0);
        ui->studentTable->setColumnCount(0);
        ui->studentTable->setEnabled(false);
        ui->addStudentPushButton->setEnabled(false);
        ui->saveSurveyFilePushButton->setEnabled(false);
        ui->actionSave_Survey_File->setEnabled(false);
        ui->teamDataLayout->setEnabled(false);
        teamDataTree->setEnabled(false);
        ui->label_23->setEnabled(false);
        ui->expandAllButton->setEnabled(false);
        ui->collapseAllButton->setEnabled(false);
        ui->label_14->setEnabled(false);
        ui->teamNamesComboBox->setEnabled(false);
        ui->dataDisplayTabWidget->setCurrentIndex(0);
        ui->isolatedWomenCheckBox->setEnabled(false);
        ui->isolatedMenCheckBox->setEnabled(false);
        ui->mixedGenderCheckBox->setEnabled(false);
        ui->label_15->setEnabled(false);
        ui->isolatedURMCheckBox->setEnabled(false);
        ui->URMResponsesButton->setEnabled(false);
        ui->label_24->setEnabled(false);
        ui->teamSizeBox->clear();
        ui->teamSizeBox->setEnabled(false);
        ui->label_20->setEnabled(false);
        ui->label_10->setEnabled(false);
        ui->idealTeamSizeBox->setEnabled(false);
        ui->letsDoItButton->setEnabled(false);
        ui->actionCreate_Teams->setEnabled(false);
        ui->saveTeamsButton->setEnabled(false);
        ui->printTeamsButton->setEnabled(false);
        ui->actionSave_Teams->setEnabled(false);
        ui->actionPrint_Teams->setEnabled(false);

        //reset the data
        delete[] student;
        student = new StudentRecord[maxStudents];
        dataOptions.dataFile = QFileInfo(fileName);
        dataOptions.numStudentsInSystem = 0;
        dataOptions.numAttributes = 0;
        dataOptions.attributeQuestionText.clear();
        for(int attrib = 0; attrib < maxAttributes; attrib++)
        {
            dataOptions.attributeQuestionResponses[attrib].clear();
            dataOptions.attributeMin[attrib] = 1;
            dataOptions.attributeMax[attrib] = 1;
            dataOptions.attributeIsOrdered[attrib] = false;
            haveAnyIncompatibleAttributes[attrib] = false;
        }
        dataOptions.scheduleDataIsFreetime = false;
        dataOptions.dayNames.clear();
        dataOptions.timeNames.clear();
        dataOptions.URMResponses.clear();
        teamingOptions.URMResponsesConsideredUR.clear();
        haveAnyRequiredTeammates = false;
        haveAnyPreventedTeammates = false;
        haveAnyRequestedTeammates = false;

        if(loadSurveyData(fileName))
        {
            // Check for duplicate student names and/or emails; warn if found
            QStringList studentNames, studentEmails;
            bool duplicatesExist = false;
            for(int ID = 0; ID < dataOptions.numStudentsInSystem; ID++)
            {
                if(!studentNames.contains(student[ID].firstname + student[ID].lastname))
                {
                    studentNames << (student[ID].firstname + student[ID].lastname);
                }
                else
                {
                    duplicatesExist = true;
                }
                if(!studentEmails.contains(student[ID].email))
                {
                    studentEmails << student[ID].email;
                }
                else
                {
                    duplicatesExist = true;
                }
            }
            if(duplicatesExist)
            {
                QMessageBox::warning(this, tr("Possible duplicate submissions"), tr("There appears to be at least one student with multiple survey submissions.\n"
                                                                                    "Possible duplicates are marked with a yellow background in the table."));
            }

            ui->statusBar->showMessage("File: " + dataOptions.dataFile.fileName());
            ui->studentTable->setEnabled(true);
            ui->addStudentPushButton->setEnabled(true);
            ui->teamDataLayout->setEnabled(true);
            ui->requiredTeammatesButton->setEnabled(true);
            ui->label_18->setEnabled(true);
            ui->preventedTeammatesButton->setEnabled(true);
            ui->requestedTeammatesButton->setEnabled(true);
            ui->label_11->setEnabled(true);
            ui->requestedTeammateNumberBox->setEnabled(true);

            ui->sectionSelectionBox->blockSignals(true);
            if(dataOptions.sectionIncluded)
            {
                //get number of sections
                QStringList sectionNames;
                for(int ID = 0; ID < numStudents; ID++)
                {
                    if(!sectionNames.contains(student[ID].section))
                    {
                        sectionNames.append(student[ID].section);
                    }
                }
                if(sectionNames.size() > 1)
                {
                    QCollator sortAlphanumerically;
                    sortAlphanumerically.setNumericMode(true);
                    sortAlphanumerically.setCaseSensitivity(Qt::CaseInsensitive);
                    std::sort(sectionNames.begin(), sectionNames.end(), sortAlphanumerically);
                    ui->sectionSelectionBox->setEnabled(true);
                    ui->label_2->setEnabled(true);
                    ui->label_22->setEnabled(true);
                    ui->sectionSelectionBox->addItem(tr("Students in all sections together"));
                    ui->sectionSelectionBox->insertSeparator(1);
                    ui->sectionSelectionBox->addItems(sectionNames);
                }
                else
                {
                    ui->sectionSelectionBox->addItem(tr("Only one section in the data."));
                }
            }
            else
            {
                ui->sectionSelectionBox->addItem(tr("No section data."));
            }
            sectionName = ui->sectionSelectionBox->currentText();
            ui->sectionSelectionBox->blockSignals(false);

            refreshStudentDisplay();
            ui->studentTable->sortByColumn(0, Qt::AscendingOrder);
            ui->studentTable->horizontalHeader()->setSortIndicatorShown(true);
            ui->studentTable->horizontalHeader()->setSortIndicator(0, Qt::AscendingOrder);
            ui->studentTable->horizontalHeaderItem(0)->setIcon(QIcon(":/icons/blank_arrow.png"));

            ui->idealTeamSizeBox->setMaximum(std::max(2,numStudents/2));
            on_idealTeamSizeBox_valueChanged(ui->idealTeamSizeBox->value());    // load new team sizes in selection box

            if(dataOptions.numAttributes > 0)
            {
                //(re)set the weight to zero for any attributes with just one value in the data
                for(int attribute = 0; attribute < dataOptions.numAttributes; attribute++)
                {
                    if(dataOptions.attributeMin[attribute] == dataOptions.attributeMax[attribute])
                    {
                        teamingOptions.attributeWeights[attribute] = 0;
                    }
                }
                ui->attributeScrollBar->setMinimum(0);
                ui->attributeScrollBar->setMaximum(dataOptions.numAttributes-1);
                ui->attributeScrollBar->setEnabled(dataOptions.numAttributes > 1);
                ui->attributeScrollBar->setValue(0);
                on_attributeScrollBar_valueChanged(0);
                ui->attributeLabel->setText(tr("Attribute  1  of  ") + QString::number(dataOptions.numAttributes));
                ui->attributeLabel->setEnabled(true);
                ui->attributeTextEdit->setEnabled(true);
                ui->attributeWeight->setEnabled(true);
                ui->attributeHomogeneousBox->setEnabled(true);
                ui->incompatibleResponsesButton->setEnabled(true);
                ui->label_21->setEnabled(true);
                ui->label_5->setEnabled(true);
            }
            else
            {
                ui->attributeScrollBar->setMaximum(-1);     // auto-sets the value and the minimum to all equal -1
            }

            if(dataOptions.genderIncluded)
            {
                ui->isolatedWomenCheckBox->setEnabled(true);
                ui->isolatedMenCheckBox->setEnabled(true);
                ui->mixedGenderCheckBox->setEnabled(true);
                ui->label_15->setEnabled(true);
            }

            if(dataOptions.URMIncluded)
            {
                ui->isolatedURMCheckBox->setEnabled(true);
                ui->URMResponsesButton->setEnabled(true);
                ui->label_24->setEnabled(true);
            }

            if(!dataOptions.dayNames.isEmpty())
            {
                ui->minMeetingTimes->setEnabled(true);
                ui->desiredMeetingTimes->setEnabled(true);
                ui->meetingLength->setEnabled(true);
                ui->scheduleWeight->setEnabled(true);
                ui->label_16->setEnabled(true);
                ui->label_0->setEnabled(true);
                ui->label_6->setEnabled(true);
                ui->label_7->setEnabled(true);
                ui->label_8->setEnabled(true);
                ui->label_9->setEnabled(true);
            }

            ui->idealTeamSizeBox->setEnabled(true);
            ui->teamSizeBox->setEnabled(true);
            ui->label_20->setEnabled(true);
            ui->label_10->setEnabled(true);
            on_idealTeamSizeBox_valueChanged(ui->idealTeamSizeBox->value());    // load new team sizes in selection box, if necessary

            ui->actionLoad_Teaming_Options_File->setEnabled(true);
            ui->actionSave_Teaming_Options_File->setEnabled(true);
            ui->letsDoItButton->setEnabled(true);
            ui->actionCreate_Teams->setEnabled(true);
        }

        QApplication::restoreOverrideCursor();
    }
}


void gruepr::loadOptionsFile()
{
    //read all options from a text file
    QString fileName = QFileDialog::getOpenFileName(this, tr("Open File"), dataOptions.dataFile.canonicalFilePath(), tr("gruepr Settings File (*.json);;All Files (*)"));
    if( !(fileName.isEmpty()) )
    {
        QFile loadFile(fileName);
        if(loadFile.open(QIODevice::ReadOnly))
        {
            QJsonDocument loadDoc(QJsonDocument::fromJson(loadFile.readAll()));
            QJsonObject loadObject = loadDoc.object();

            if(loadObject.contains("idealTeamSize") && loadObject["idealTeamSize"].isDouble())
            {
                ui->idealTeamSizeBox->setValue(loadObject["idealTeamSize"].toInt());
                on_idealTeamSizeBox_valueChanged(ui->idealTeamSizeBox->value());    // load new team sizes in selection box, if necessary
            }
            if(loadObject.contains("isolatedWomenPrevented") && loadObject["isolatedWomenPrevented"].isBool())
            {
                teamingOptions.isolatedWomenPrevented = loadObject["isolatedWomenPrevented"].toBool();
                ui->isolatedWomenCheckBox->setChecked(teamingOptions.isolatedWomenPrevented);
            }
            if(loadObject.contains("isolatedMenPrevented") && loadObject["isolatedMenPrevented"].isBool())
            {
                teamingOptions.isolatedMenPrevented = loadObject["isolatedMenPrevented"].toBool();
                ui->isolatedMenCheckBox->setChecked(teamingOptions.isolatedMenPrevented);
            }
            if(loadObject.contains("singleGenderPrevented") && loadObject["singleGenderPrevented"].isBool())
            {
                teamingOptions.singleGenderPrevented = loadObject["singleGenderPrevented"].toBool();
                ui->mixedGenderCheckBox->setChecked(teamingOptions.singleGenderPrevented);
            }
            if(loadObject.contains("isolatedURMPrevented") && loadObject["isolatedURMPrevented"].isBool())
            {
                teamingOptions.isolatedURMPrevented = loadObject["isolatedURMPrevented"].toBool();
                ui->isolatedURMCheckBox->blockSignals(true);    // prevent select URM identities box from immediately opening
                ui->isolatedURMCheckBox->setChecked(teamingOptions.isolatedURMPrevented);
                ui->isolatedURMCheckBox->blockSignals(false);
            }
            if(loadObject.contains("URMResponsesConsideredUR") && loadObject["URMResponsesConsideredUR"].isString())
            {
                teamingOptions.URMResponsesConsideredUR = loadObject["URMResponsesConsideredUR"].toString().split(';');
            }
            if(loadObject.contains("minTimeBlocksOverlap") && loadObject["minTimeBlocksOverlap"].isDouble())
            {
                teamingOptions.minTimeBlocksOverlap = loadObject["minTimeBlocksOverlap"].toInt();
                ui->minMeetingTimes->setValue(teamingOptions.minTimeBlocksOverlap);
            }
            if(loadObject.contains("desiredTimeBlocksOverlap") && loadObject["desiredTimeBlocksOverlap"].isDouble())
            {
                teamingOptions.desiredTimeBlocksOverlap = loadObject["desiredTimeBlocksOverlap"].toInt();
                ui->desiredMeetingTimes->setValue(teamingOptions.desiredTimeBlocksOverlap);
            }
            if(loadObject.contains("meetingBlockSize") && loadObject["meetingBlockSize"].isDouble())
            {
                teamingOptions.meetingBlockSize = loadObject["meetingBlockSize"].toInt();
                ui->meetingLength->setCurrentIndex(teamingOptions.meetingBlockSize - 1);
            }
            if(loadObject.contains("scheduleWeight") && loadObject["scheduleWeight"].isDouble())
            {
                teamingOptions.scheduleWeight = float(loadObject["scheduleWeight"].toDouble());
                ui->scheduleWeight->setValue(teamingOptions.scheduleWeight);
            }

            for(int attribute = 0; attribute < maxAttributes; attribute++)
            {
                if(loadObject.contains("Attribute" + QString::number(attribute+1)+"desireHomogeneous") &&
                        loadObject["Attribute" + QString::number(attribute+1)+"desireHomogeneous"].isBool())
                {
                    teamingOptions.desireHomogeneous[attribute] = loadObject["Attribute" + QString::number(attribute+1)+"desireHomogeneous"].toBool();
                }
                if(loadObject.contains("Attribute" + QString::number(attribute+1)+"Weight") &&
                        loadObject["Attribute" + QString::number(attribute+1)+"Weight"].isDouble())
                {
                    teamingOptions.attributeWeights[attribute] = float(loadObject["Attribute" + QString::number(attribute+1)+"Weight"].toDouble());
                    //reset the weight to zero for any attributes with just one value in the data
                    if(dataOptions.attributeMin[attribute] == dataOptions.attributeMax[attribute])
                    {
                        teamingOptions.attributeWeights[attribute] = 0;
                    }
                }
                int incompatibleResponseNum = 0;
                QList< QPair<int,int> > setOfIncompatibleResponses;
                while(loadObject.contains("Attribute" + QString::number(attribute+1) + "incompatibleResponse" + QString::number(incompatibleResponseNum+1)) &&
                      loadObject["Attribute" + QString::number(attribute+1) + "incompatibleResponse" + QString::number(incompatibleResponseNum+1)].isString())
                {
                    QStringList incoRes = loadObject["Attribute" + QString::number(attribute+1) + "incompatibleResponse" + QString::number(incompatibleResponseNum+1)].toString().split(',');
                    setOfIncompatibleResponses << QPair<int,int>(incoRes.at(0).toInt(),incoRes.at(1).toInt());
                    incompatibleResponseNum++;
                }
                teamingOptions.incompatibleAttributeValues[attribute] = setOfIncompatibleResponses;
            }
            if(ui->attributeScrollBar->value() == 0)
            {
                on_attributeScrollBar_valueChanged(0);      // displays the correct attribute weight, homogeneity, text in case scrollbar is already at 0
            }
            else
            {
                ui->attributeScrollBar->setValue(0);
            }

            if(loadObject.contains("numberRequestedTeammatesGiven") && loadObject["numberRequestedTeammatesGiven"].isDouble())
            {
                teamingOptions.numberRequestedTeammatesGiven = loadObject["numberRequestedTeammatesGiven"].toInt();
                ui->requestedTeammateNumberBox->setValue(teamingOptions.numberRequestedTeammatesGiven);
            }

            loadFile.close();
        }
        else
        {
            QMessageBox::critical(this, tr("File Error"), tr("This file cannot be read."));
        }
    }
}


void gruepr::saveOptionsFile()
{
    QString fileName = QFileDialog::getSaveFileName(this, tr("Save File"), dataOptions.dataFile.canonicalPath(), tr("gruepr Settings File (*.json);;All Files (*)"));
    if( !(fileName.isEmpty()) )
    {
        QFile saveFile(fileName);
        if(saveFile.open(QIODevice::WriteOnly | QIODevice::Text))
        {
            QJsonObject saveObject;
            saveObject["idealTeamSize"] = ui->idealTeamSizeBox->value();
            saveObject["isolatedWomenPrevented"] = teamingOptions.isolatedWomenPrevented;
            saveObject["isolatedMenPrevented"] = teamingOptions.isolatedMenPrevented;
            saveObject["singleGenderPrevented"] = teamingOptions.singleGenderPrevented;
            saveObject["isolatedURMPrevented"] = teamingOptions.isolatedURMPrevented;
            saveObject["URMResponsesConsideredUR"] = teamingOptions.URMResponsesConsideredUR.join(';');
            saveObject["minTimeBlocksOverlap"] = teamingOptions.minTimeBlocksOverlap;
            saveObject["desiredTimeBlocksOverlap"] = teamingOptions.desiredTimeBlocksOverlap;
            saveObject["meetingBlockSize"] = teamingOptions.meetingBlockSize;
            saveObject["scheduleWeight"] = teamingOptions.scheduleWeight;
            for(int attribute = 0; attribute < maxAttributes; attribute++)
            {
                saveObject["Attribute" + QString::number(attribute+1)+"desireHomogeneous"] = teamingOptions.desireHomogeneous[attribute];
                saveObject["Attribute" + QString::number(attribute+1)+"Weight"] = teamingOptions.attributeWeights[attribute];
                for(int incompResp = 0; incompResp < teamingOptions.incompatibleAttributeValues[attribute].size(); incompResp++)
                {
                    saveObject["Attribute" + QString::number(attribute+1)+"incompatibleResponse" + QString::number(incompResp+1)] =
                            (QString::number(teamingOptions.incompatibleAttributeValues[attribute].at(incompResp).first) + "," +
                             QString::number(teamingOptions.incompatibleAttributeValues[attribute].at(incompResp).second));
                }
            }
            saveObject["numberRequestedTeammatesGiven"] = teamingOptions.numberRequestedTeammatesGiven;

            QJsonDocument saveDoc(saveObject);
            saveFile.write(saveDoc.toJson());
            saveFile.close();
        }
        else
        {
            QMessageBox::critical(this, tr("No Files Saved"), tr("This settings file was not saved.\nThere was an issue writing the file to disk."));
        }
    }
}


void gruepr::on_sectionSelectionBox_currentIndexChanged(const QString &desiredSection)
{
    if(desiredSection == "")
    {
        numStudents = 0;
        return;
    }

    sectionName = desiredSection;

    refreshStudentDisplay();
    ui->studentTable->horizontalHeaderItem(ui->studentTable->horizontalHeader()->sortIndicatorSection())->setIcon(QIcon(":/icons/blank_arrow.png"));

    ui->idealTeamSizeBox->setMaximum(std::max(2,numStudents/2));
    on_idealTeamSizeBox_valueChanged(ui->idealTeamSizeBox->value());    // load new team sizes in selection box, if necessary
}


void gruepr::on_studentTable_cellEntered(int row, int column)
{
    (void)column;
    // select the current row, reset the background color of the edit and remover buttons in previously selected row and change their color in the current row
    ui->studentTable->selectRow(row);
    static int prevID = -1;
    if(prevID != -1)
    {
        int prevRow = 0;
        while((prevRow < ui->studentTable->rowCount()) && (prevID != ui->studentTable->cellWidget(prevRow, ui->studentTable->columnCount()-1)->property("StudentID").toInt()))
        {
            prevRow++;
        }
        if(prevRow < ui->studentTable->rowCount())
        {
            if(ui->studentTable->cellWidget(prevRow, ui->studentTable->columnCount()-1)->property("duplicate").toBool())
            {
                ui->studentTable->cellWidget(prevRow, ui->studentTable->columnCount()-1)->setStyleSheet("QPushButton {background-color: #ffff3b; border: none;}");
                ui->studentTable->cellWidget(prevRow, ui->studentTable->columnCount()-2)->setStyleSheet("QPushButton {background-color: #ffff3b; border: none;}");
            }
            else
            {
                ui->studentTable->cellWidget(prevRow, ui->studentTable->columnCount()-1)->setStyleSheet("");
                ui->studentTable->cellWidget(prevRow, ui->studentTable->columnCount()-2)->setStyleSheet("");
            }
        }
    }
    prevID = ui->studentTable->cellWidget(row, ui->studentTable->columnCount()-1)->property("StudentID").toInt();
    ui->studentTable->cellWidget(row, ui->studentTable->columnCount()-1)->setStyleSheet("QPushButton {background-color: #85cbf8; border: none;}");
    ui->studentTable->cellWidget(row, ui->studentTable->columnCount()-2)->setStyleSheet("QPushButton {background-color: #85cbf8; border: none;}");
}


void gruepr::editAStudent()
{
    //Find the student record
    int ID = 0;
    while(sender()->property("StudentID").toInt() != student[ID].ID)
    {
        ID++;
    }

    QStringList sectionNames;
    if(dataOptions.sectionIncluded)
    {
        for(int ID = 0; ID < dataOptions.numStudentsInSystem; ID++)
        {
            if(!sectionNames.contains(student[ID].section))
            {
                sectionNames.append(student[ID].section);
            }
        }
    }
    QCollator sortAlphanumerically;
    sortAlphanumerically.setNumericMode(true);
    sortAlphanumerically.setCaseSensitivity(Qt::CaseInsensitive);
    std::sort(sectionNames.begin(), sectionNames.end(), sortAlphanumerically);

    //Open window with the student record in it
    auto *win = new editOrAddStudentDialog(student[ID], dataOptions, sectionNames, this);

    //If user clicks OK, replace student in the database with edited copy
    int reply = win->exec();
    if(reply == QDialog::Accepted)
    {
        student[ID] = win->student;
        student[ID].URM = teamingOptions.URMResponsesConsideredUR.contains(student[ID].URMResponse);

        //Re-build the URM info
        if(dataOptions.URMIncluded)
        {
            dataOptions.URMResponses.clear();
            for(int ID = 0; ID < dataOptions.numStudentsInSystem; ID++)
            {
                if(!dataOptions.URMResponses.contains(student[ID].URMResponse, Qt::CaseInsensitive))
                {
                    dataOptions.URMResponses << student[ID].URMResponse;
                }
            }
            QCollator sortAlphanumerically;
            sortAlphanumerically.setNumericMode(true);
            sortAlphanumerically.setCaseSensitivity(Qt::CaseInsensitive);
            std::sort(dataOptions.URMResponses.begin(), dataOptions.URMResponses.end(), sortAlphanumerically);
            if(dataOptions.URMResponses.contains("--"))
            {
                // put the blank response option at the end of the list
                dataOptions.URMResponses.removeAll("--");
                dataOptions.URMResponses << "--";
            }
        }

        //Re-build the section options in the selection box (in case user added a new section name)
        if(dataOptions.sectionIncluded)
        {
            QString currentSection = ui->sectionSelectionBox->currentText();
            ui->sectionSelectionBox->clear();
            //get number of sections
            QStringList newSectionNames;
            for(int ID = 0; ID < dataOptions.numStudentsInSystem; ID++)
            {
                if(!newSectionNames.contains(student[ID].section))
                {
                    newSectionNames.append(student[ID].section);
                }
            }
            if(newSectionNames.size() > 1)
            {
                QCollator sortAlphanumerically;
                sortAlphanumerically.setNumericMode(true);
                sortAlphanumerically.setCaseSensitivity(Qt::CaseInsensitive);
                std::sort(newSectionNames.begin(), newSectionNames.end(), sortAlphanumerically);
                ui->sectionSelectionBox->setEnabled(true);
                ui->label_2->setEnabled(true);
                ui->label_22->setEnabled(true);
                ui->sectionSelectionBox->addItem(tr("Students in all sections together"));
                ui->sectionSelectionBox->insertSeparator(1);
                ui->sectionSelectionBox->addItems(newSectionNames);
            }
            else
            {
                ui->sectionSelectionBox->addItem(tr("Only one section in the data."));
            }

            if(ui->sectionSelectionBox->findText(currentSection) != -1)
            {
                ui->sectionSelectionBox->setCurrentText(currentSection);
            }
        }

        //Enable save data file option, since data set is now edited
        ui->saveSurveyFilePushButton->setEnabled(true);
        ui->actionSave_Survey_File->setEnabled(true);

        refreshStudentDisplay();
        ui->studentTable->horizontalHeaderItem(ui->studentTable->horizontalHeader()->sortIndicatorSection())->setIcon(QIcon(":/icons/blank_arrow.png"));
    }

    delete win;
}


void gruepr::removeAStudent()
{
    //Search through all the students and, once we found the one with the matching ID, move all remaining ones ahead by one and decrement numStudentsInSystem
    bool foundIt = false;
    for(int ID = 0; ID < dataOptions.numStudentsInSystem; ID++)
    {
        if(sender()->property("StudentID").toInt() == student[ID].ID)
        {
            foundIt = true;
        }
        if(foundIt)
        {
            student[ID] = student[ID+1];
            student[ID].ID = ID;
        }
    }
    dataOptions.numStudentsInSystem--;

    //Enable save data file option, since data set is now edited
    ui->saveSurveyFilePushButton->setEnabled(true);
    ui->actionSave_Survey_File->setEnabled(true);

    refreshStudentDisplay();
    ui->studentTable->horizontalHeaderItem(ui->studentTable->horizontalHeader()->sortIndicatorSection())->setIcon(QIcon(":/icons/blank_arrow.png"));

    ui->idealTeamSizeBox->setMaximum(std::max(2,numStudents/2));
    on_idealTeamSizeBox_valueChanged(ui->idealTeamSizeBox->value());    // load new team sizes in selection box, if necessary
}


void gruepr::on_addStudentPushButton_clicked()
{
    if(dataOptions.numStudentsInSystem < maxStudents)
    {
        QStringList sectionNames;
        if(dataOptions.sectionIncluded)
        {
            for(int ID = 0; ID < numStudents; ID++)
            {
                if(!sectionNames.contains(student[ID].section))
                {
                    sectionNames.append(student[ID].section);
                }
            }
        }

        //Open window with a blank student record in it
        StudentRecord newStudent;
        newStudent.ID = dataOptions.numStudentsInSystem;
        newStudent.surveyTimestamp = QDateTime::currentDateTime();
        auto *win = new editOrAddStudentDialog(newStudent, dataOptions, sectionNames, this);

        //If user clicks OK, add student to the database
        int reply = win->exec();
        if(reply == QDialog::Accepted)
        {
            student[dataOptions.numStudentsInSystem] = win->student;
            student[dataOptions.numStudentsInSystem].URM = teamingOptions.URMResponsesConsideredUR.contains(student[dataOptions.numStudentsInSystem].URMResponse);

            dataOptions.numStudentsInSystem++;

            //Re-build the URM info
            if(dataOptions.URMIncluded)
            {
                dataOptions.URMResponses.clear();
                for(int ID = 0; ID < dataOptions.numStudentsInSystem; ID++)
                {
                    if(!dataOptions.URMResponses.contains(student[ID].URMResponse, Qt::CaseInsensitive))
                    {
                        dataOptions.URMResponses << student[ID].URMResponse;
                    }
                }
                QCollator sortAlphanumerically;
                sortAlphanumerically.setNumericMode(true);
                sortAlphanumerically.setCaseSensitivity(Qt::CaseInsensitive);
                std::sort(dataOptions.URMResponses.begin(), dataOptions.URMResponses.end(), sortAlphanumerically);
                if(dataOptions.URMResponses.contains("--"))
                {
                    // put the blank response option at the end of the list
                    dataOptions.URMResponses.removeAll("--");
                    dataOptions.URMResponses << "--";
                }
            }

            //Re-do the section options in the selection box (in case user added a new section name)
            if(dataOptions.sectionIncluded)
            {
                QString currentSection = ui->sectionSelectionBox->currentText();
                ui->sectionSelectionBox->clear();
                //get number of sections
                QStringList newSectionNames;
                for(int ID = 0; ID < dataOptions.numStudentsInSystem; ID++)
                {
                    if(!newSectionNames.contains(student[ID].section))
                    {
                        newSectionNames.append(student[ID].section);
                    }
                }
                if(newSectionNames.size() > 1)
                {
                    QCollator sortAlphanumerically;
                    sortAlphanumerically.setNumericMode(true);
                    sortAlphanumerically.setCaseSensitivity(Qt::CaseInsensitive);
                    std::sort(newSectionNames.begin(), newSectionNames.end(), sortAlphanumerically);
                    ui->sectionSelectionBox->setEnabled(true);
                    ui->label_2->setEnabled(true);
                    ui->label_22->setEnabled(true);
                    ui->sectionSelectionBox->addItem(tr("Students in all sections together"));
                    ui->sectionSelectionBox->insertSeparator(1);
                    ui->sectionSelectionBox->addItems(newSectionNames);
                }
                else
                {
                    ui->sectionSelectionBox->addItem(tr("Only one section in the data."));
                }

                if(ui->sectionSelectionBox->findText(currentSection) != -1)
                {
                    ui->sectionSelectionBox->setCurrentText(currentSection);
                }
            }

            //Enable save data file option, since data set is now edited
            ui->saveSurveyFilePushButton->setEnabled(true);
            ui->actionSave_Survey_File->setEnabled(true);

            refreshStudentDisplay();
            ui->studentTable->horizontalHeaderItem(ui->studentTable->horizontalHeader()->sortIndicatorSection())->setIcon(QIcon(":/icons/blank_arrow.png"));

            ui->idealTeamSizeBox->setMaximum(std::max(2,numStudents/2));
            on_idealTeamSizeBox_valueChanged(ui->idealTeamSizeBox->value());    // load new team sizes in selection box
        }

        delete win;
    }
    else
    {
        QMessageBox::warning(this, tr("Cannot add student."),
                             tr("Sorry, we cannot add another student.\nThis version of gruepr does not allow more than ") + QString(maxStudents) + tr("."), QMessageBox::Ok);
    }
}


void gruepr::on_saveSurveyFilePushButton_clicked()
{
    QString newSurveyFileName = QFileDialog::getSaveFileName(this, tr("Save Survey Data File"), dataOptions.dataFile.canonicalPath(), tr("Survey Data File (*.csv);;All Files (*)"));
    if ( !(newSurveyFileName.isEmpty()) )
    {
        bool problemSaving = false;
        QFile newSurveyFile(newSurveyFileName);
        if(newSurveyFile.open(QIODevice::WriteOnly | QIODevice::Text))
        {
            QTextStream out(&newSurveyFile);

            // Write the header row
            out << "Timestamp,What is your first name (or the name you prefer to be called)?,What is your last name?,What is your email address?";       // need at least timestamp, first name, last name, email address
            if(dataOptions.genderIncluded)
            {
                out << ",With which gender do you identify?";
            }
            // See if URM data is included
            if(dataOptions.URMIncluded)
            {
                 out << ",How do you identify your race, ethnicity, or cultural heritage?";
            }
            for(int attrib = 0; attrib < dataOptions.numAttributes; attrib++)
            {
                out << ",\"" << dataOptions.attributeQuestionText[attrib] << "\"";
            }
            for(int day = 0; day < dataOptions.dayNames.size(); day++)
            {
                if(dataOptions.scheduleDataIsFreetime)
                {
                    out << ",Check the times that you are FREE and will be AVAILABLE for group work. [" << dataOptions.dayNames[day] << "]";
                }
                else
                {
                    out << ",Check the times that you are BUSY and will be UNAVAILABLE for group work. [" << dataOptions.dayNames[day] << "]";
                }
            }
            if(dataOptions.sectionIncluded)
            {
                out << ",In which section are you enrolled?";
            }
            if(dataOptions.notesIncluded)
            {
                out << ",Notes";
            }
            out << endl;

            // Add each student's info
            for(int ID = 0; ID < dataOptions.numStudentsInSystem; ID++)
            {
                out << student[ID].surveyTimestamp.toString(Qt::ISODate) << ",\"" << student[ID].firstname << "\",\"" << student[ID].lastname << "\",\"" << student[ID].email << "\"";
                if(dataOptions.genderIncluded)
                {
                    out << "," << (student[ID].gender == StudentRecord::woman? tr("woman") : (student[ID].gender == StudentRecord::man? tr("man") : tr("nonbinary/unknown")));
                }
                if(dataOptions.URMIncluded)
                {
                    out << "," << (student[ID].URMResponse);
                }
                for(int attrib = 0; attrib < dataOptions.numAttributes; attrib++)
                {
                    out << ",\"" << student[ID].attributeResponse[attrib] << "\"";
                }
                for(int day = 0; day < dataOptions.dayNames.size(); day++)
                {
                    out << ",\"";
                    bool firsttime = true;
                    for(int time = 0; time < dataOptions.timeNames.size(); time++)
                    {
                        if(dataOptions.scheduleDataIsFreetime)
                        {
                            if(!student[ID].unavailable[(day*dataOptions.timeNames.size())+time])
                            {
                                out << (firsttime? "" : ", ");
                                out << dataOptions.timeNames.at(time);
                                firsttime = false;
                            }
                        }
                        else
                        {
                            if(student[ID].unavailable[(day*dataOptions.timeNames.size())+time])
                            {
                                out << (firsttime? "" : ", ");
                                out << dataOptions.timeNames.at(time);
                                firsttime = false;
                            }
                        }
                    }
                    out << "\"";
                }
                if(dataOptions.sectionIncluded)
                {
                    out << ",\"" << student[ID].section << "\"";
                }
                if(dataOptions.notesIncluded)
                {
                    out << ",\"" << student[ID].notes << "\"";
                }
                out << endl;
            }

            newSurveyFile.close();
        }
        else
        {
            problemSaving = true;
        }

        if(problemSaving)
        {
            QMessageBox::critical(this, tr("No File Saved"), tr("No file was saved.\nThere was an issue writing the file."));
        }
        else
        {
            ui->saveSurveyFilePushButton->setEnabled(false);
            ui->actionSave_Survey_File->setEnabled(false);
        }
    }
}


void gruepr::on_isolatedWomenCheckBox_stateChanged(int arg1)
{
    teamingOptions.isolatedWomenPrevented = arg1;
}


void gruepr::on_isolatedMenCheckBox_stateChanged(int arg1)
{
    teamingOptions.isolatedMenPrevented = arg1;
}


void gruepr::on_mixedGenderCheckBox_stateChanged(int arg1)
{
    teamingOptions.singleGenderPrevented = arg1;
}


void gruepr::on_isolatedURMCheckBox_stateChanged(int arg1)
{
    teamingOptions.isolatedURMPrevented = arg1;
    if(teamingOptions.isolatedURMPrevented && teamingOptions.URMResponsesConsideredUR.isEmpty())
    {
        // if we are preventing isolated URM students, but have not selected yet which responses should be considered URM, let's ask user to enter those in
        on_URMResponsesButton_clicked();
    }
}


void gruepr::on_URMResponsesButton_clicked()
{
    // open window to decide which values are to be considered underrepresented
    auto *win = new gatherURMResponsesDialog(dataOptions, teamingOptions.URMResponsesConsideredUR, this);

    //If user clicks OK, replace the responses considered underrepresented with the set from the window
    int reply = win->exec();
    if(reply == QDialog::Accepted)
    {
        teamingOptions.URMResponsesConsideredUR = win->URMResponsesConsideredUR;
        teamingOptions.URMResponsesConsideredUR.removeDuplicates();
        //(re)apply these values to the student database
        for(int ID = 0; ID < dataOptions.numStudentsInSystem; ID++)
        {
            student[ID].URM = teamingOptions.URMResponsesConsideredUR.contains(student[ID].URMResponse);
        }
    }

    delete win;
}


void gruepr::on_attributeScrollBar_valueChanged(int value)
{
    if(value >= 0)    // needed for when scroll bar is cleared, when value gets set to -1
    {
        QString questionWithResponses = "<html>" + dataOptions.attributeQuestionText.at(value) + "<hr>" + tr("Responses:") + "<div style=\"margin-left:5%;\">";
        for(int response = 0; response < dataOptions.attributeQuestionResponses[value].size(); response++)
        {
            if(dataOptions.attributeIsOrdered[value])
            {
                // show reponse with starting number in bold
                QRegularExpression startsWithNumber("^(\\d+)(.+)");
                QRegularExpressionMatch match = startsWithNumber.match(dataOptions.attributeQuestionResponses[value].at(response));
                questionWithResponses += "<br><b>" + match.captured(1) + "</b>" + match.captured(2);
            }
            else
            {
                // show response with a preceding letter in bold (letter repeated for responses after 26)
                questionWithResponses += "<br><b>";
                questionWithResponses += (response < 26 ? QString(char(response + 'A')) : QString(char(response%26 + 'A')).repeated(1 + (response/26)));
                questionWithResponses += "</b>. " + dataOptions.attributeQuestionResponses[value].at(response);
            }
        }
        questionWithResponses += "</div></html>";
        ui->attributeTextEdit->setHtml(questionWithResponses);
        if(dataOptions.attributeMin[value] == dataOptions.attributeMax[value])
        {
            teamingOptions.attributeWeights[value] = 0;
            ui->attributeWeight->setEnabled(false);
            ui->attributeWeight->setToolTip(tr("With only one response value, this attribute cannot be used for teaming"));
            ui->attributeHomogeneousBox->setEnabled(false);
            ui->attributeHomogeneousBox->setToolTip(tr("With only one response value, this attribute cannot be used for teaming"));
            ui->incompatibleResponsesButton->setEnabled(false);
            ui->incompatibleResponsesButton->setToolTip(tr("With only one response value, this attribute cannot be used for teaming"));
        }
        else
        {
            ui->attributeWeight->setEnabled(true);
            ui->attributeWeight->setToolTip(tr("The relative importance of this attribute in forming the teams"));
            ui->attributeHomogeneousBox->setEnabled(true);
            ui->attributeHomogeneousBox->setToolTip(tr("If selected, all of the students on a team will have a similar response to this question.\n"
                                                       "If unselected, the students on a team will have a wide range of responses to this question."));
            ui->incompatibleResponsesButton->setEnabled(true);
            ui->incompatibleResponsesButton->setToolTip(tr("<html>Indicate response values that should prevent students from being on the same team.</html>"));
        }
        ui->attributeWeight->setValue(double(teamingOptions.attributeWeights[value]));
        ui->attributeHomogeneousBox->setChecked(teamingOptions.desireHomogeneous[value]);
        ui->attributeLabel->setText(tr("Attribute  ") + QString::number(value+1) + tr("  of  ") + QString::number(dataOptions.numAttributes));
    }
}


void gruepr::on_attributeWeight_valueChanged(double arg1)
{
    if(ui->attributeScrollBar->value() >= 0)    // needed for when scroll bar is cleared, when value gets set to -1
    {
        teamingOptions.attributeWeights[ui->attributeScrollBar->value()] = float(arg1);
    }
}


void gruepr::on_attributeHomogeneousBox_stateChanged(int arg1)
{
    if(ui->attributeScrollBar->value() >= 0)    // needed for when scroll bar is cleared, when value gets set to -1
    {
        teamingOptions.desireHomogeneous[ui->attributeScrollBar->value()] = arg1;
    }
}


void gruepr::on_incompatibleResponsesButton_clicked()
{
    //Open specialized dialog box to collect response pairings that should prevent students from being on the same team
    auto *win = new gatherIncompatibleResponsesDialog(ui->attributeScrollBar->value(), dataOptions, teamingOptions.incompatibleAttributeValues[ui->attributeScrollBar->value()], this);

    //If user clicks OK, replace student database with copy that has had pairings added
    int reply = win->exec();
    if(reply == QDialog::Accepted)
    {
        haveAnyIncompatibleAttributes[ui->attributeScrollBar->value()] = !(win->incompatibleResponses.isEmpty());
        teamingOptions.incompatibleAttributeValues[ui->attributeScrollBar->value()] = win->incompatibleResponses;
    }

    delete win;
}


void gruepr::on_scheduleWeight_valueChanged(double arg1)
{
    teamingOptions.scheduleWeight = float(arg1);
}


void gruepr::on_minMeetingTimes_valueChanged(int arg1)
{
    teamingOptions.minTimeBlocksOverlap = arg1;
    if(ui->desiredMeetingTimes->value() < (arg1+1))
    {
        ui->desiredMeetingTimes->setValue(arg1+1);
    }
}


void gruepr::on_desiredMeetingTimes_valueChanged(int arg1)
{
    teamingOptions.desiredTimeBlocksOverlap = arg1;
    if(ui->minMeetingTimes->value() > (arg1-1))
    {
        ui->minMeetingTimes->setValue(arg1-1);
    }
}


void gruepr::on_meetingLength_currentIndexChanged(int index)
{
    teamingOptions.meetingBlockSize = (index + 1);
}


void gruepr::on_requiredTeammatesButton_clicked()
{
    //Open specialized dialog box to collect pairings that are required
    auto *win = new gatherTeammatesDialog(gatherTeammatesDialog::required, student, dataOptions.numStudentsInSystem, (ui->sectionSelectionBox->currentIndex()==0)? "" : sectionName, this);

    //If user clicks OK, replace student database with copy that has had pairings added
    int reply = win->exec();
    if(reply == QDialog::Accepted)
    {
        for(int ID = 0; ID < dataOptions.numStudentsInSystem; ID++)
        {
            this->student[ID] = win->student[ID];
        }
        haveAnyRequiredTeammates = win->teammatesSpecified;
    }

    delete win;
}


void gruepr::on_preventedTeammatesButton_clicked()
{
    //Open specialized dialog box to collect pairings that are prevented
    auto *win = new gatherTeammatesDialog(gatherTeammatesDialog::prevented, student, dataOptions.numStudentsInSystem, (ui->sectionSelectionBox->currentIndex()==0)? "" : sectionName, this);

    //If user clicks OK, replace student database with copy that has had pairings added
    int reply = win->exec();
    if(reply == QDialog::Accepted)
    {
        for(int ID = 0; ID < dataOptions.numStudentsInSystem; ID++)
        {
            this->student[ID] = win->student[ID];
        }
        haveAnyPreventedTeammates = win->teammatesSpecified;
    }

    delete win;
}


void gruepr::on_requestedTeammatesButton_clicked()
{
    //Open specialized dialog box to collect pairings that are required
    auto *win = new gatherTeammatesDialog(gatherTeammatesDialog::requested, student, dataOptions.numStudentsInSystem, (ui->sectionSelectionBox->currentIndex()==0)? "" : sectionName, this);

    //If user clicks OK, replace student database with copy that has had pairings added
    int reply = win->exec();
    if(reply == QDialog::Accepted)
    {
        for(int ID = 0; ID < dataOptions.numStudentsInSystem; ID++)
        {
            this->student[ID] = win->student[ID];
        }
        haveAnyRequestedTeammates = win->teammatesSpecified;
    }

    delete win;
}


void gruepr::on_requestedTeammateNumberBox_valueChanged(int arg1)
{
    teamingOptions.numberRequestedTeammatesGiven = arg1;
}


void gruepr::on_idealTeamSizeBox_valueChanged(int arg1)
{
    ui->teamSizeBox->clear();

    numTeams = std::max(1, numStudents/arg1);
    teamingOptions.smallerTeamsNumTeams = numTeams;
    teamingOptions.largerTeamsNumTeams = numTeams;

    if(numStudents%arg1 != 0)       //if teams can't be evenly divided into this size
    {
        int smallerTeamsSizeA=0, smallerTeamsSizeB=0, numSmallerATeams=0, largerTeamsSizeA=0, largerTeamsSizeB=0, numLargerATeams=0;

        // reset the potential team sizes
        for(int student = 0; student < maxStudents; student++)
        {
            teamingOptions.smallerTeamsSizes[student] = 0;
            teamingOptions.largerTeamsSizes[student] = 0;
        }

        // What are the team sizes when desiredTeamSize represents a maximum size?
        teamingOptions.smallerTeamsNumTeams = numTeams+1;
        for(int student = 0; student < numStudents; student++)      // run through every student
        {
            // add one student to each team (with 1 additional team relative to before) in turn until we run out of students
            (teamingOptions.smallerTeamsSizes[student%teamingOptions.smallerTeamsNumTeams])++;
            smallerTeamsSizeA = teamingOptions.smallerTeamsSizes[student%teamingOptions.smallerTeamsNumTeams];  // the larger of the two (uneven) team sizes
            numSmallerATeams = (student%teamingOptions.smallerTeamsNumTeams)+1;                                 // the number of larger teams
        }
        smallerTeamsSizeB = smallerTeamsSizeA - 1;                  // the smaller of the two (uneven) team sizes

        // And what are the team sizes when desiredTeamSize represents a minimum size?
        teamingOptions.largerTeamsNumTeams = numTeams;
        for(int student = 0; student < numStudents; student++)	// run through every student
        {
            // add one student to each team in turn until we run out of students
            (teamingOptions.largerTeamsSizes[student%teamingOptions.largerTeamsNumTeams])++;
            largerTeamsSizeA = teamingOptions.largerTeamsSizes[student%teamingOptions.largerTeamsNumTeams];     // the larger of the two (uneven) team sizes
            numLargerATeams = (student%teamingOptions.largerTeamsNumTeams)+1;                                   // the number of larger teams
        }
        largerTeamsSizeB = largerTeamsSizeA - 1;					// the smaller of the two (uneven) team sizes

        // Add first option to selection box
        QString smallerTeamOption;
        if(numSmallerATeams > 0)
        {
            smallerTeamOption += QString::number(numSmallerATeams) + tr(" team");
            if(numSmallerATeams > 1)
            {
                smallerTeamOption += "s";
            }
            smallerTeamOption += " of " + QString::number(smallerTeamsSizeA) + tr(" student");
            if(smallerTeamsSizeA > 1)
            {
                smallerTeamOption += "s";
            }
        }
        if((numSmallerATeams > 0) && ((numTeams+1-numSmallerATeams) > 0))
        {
            smallerTeamOption += " + ";
        }
        if((numTeams+1-numSmallerATeams) > 0)
        {
            smallerTeamOption += QString::number(numTeams+1-numSmallerATeams) + tr(" team");
            if((numTeams+1-numSmallerATeams) > 1)
            {
                smallerTeamOption += "s";
            }
            smallerTeamOption += " of " + QString::number(smallerTeamsSizeB) + tr(" student");
            if(smallerTeamsSizeB > 1)
            {
                smallerTeamOption += "s";
            }
        }

        // Add second option to selection box
        QString largerTeamOption;
        if((numTeams-numLargerATeams) > 0)
        {
            largerTeamOption += QString::number(numTeams-numLargerATeams) + tr(" team");
            if((numTeams-numLargerATeams) > 1)
            {
                largerTeamOption += "s";
            }
            largerTeamOption += " of " + QString::number(largerTeamsSizeB) + tr(" student");
            if(largerTeamsSizeB > 1)
            {
                largerTeamOption += "s";
            }
        }
        if(((numTeams-numLargerATeams) > 0) && (numLargerATeams > 0))
        {
            largerTeamOption += " + ";
        }
        if(numLargerATeams > 0)
        {
            largerTeamOption += QString::number(numLargerATeams) + tr(" team");
            if(numLargerATeams > 1)
            {
                largerTeamOption += "s";
            }
            largerTeamOption += " of " + QString::number(largerTeamsSizeA) + tr(" student");
            if(largerTeamsSizeA > 1)
            {
                largerTeamOption += "s";
            }
        }

        ui->teamSizeBox->addItem(smallerTeamOption);
        ui->teamSizeBox->addItem(largerTeamOption);
    }
    else
    {
        ui->teamSizeBox->addItem(QString::number(numTeams) + tr(" teams of ") + QString::number(arg1) + tr(" students"));
    }
    ui->teamSizeBox->insertSeparator(ui->teamSizeBox->count());
    ui->teamSizeBox->addItem(tr("Custom team sizes"));
}


void gruepr::on_teamSizeBox_currentIndexChanged(int index)
{
    if(ui->teamSizeBox->currentText() == (QString::number(numTeams) + tr(" teams of ") + QString::number(ui->idealTeamSizeBox->value()) + tr(" students")))
    {
        // Evenly divisible teams, all same size
        setTeamSizes(ui->idealTeamSizeBox->value());
    }
    else if(ui->teamSizeBox->currentText() == tr("Custom team sizes"))
    {
        //Open specialized dialog box to collect teamsizes
        auto *win = new customTeamsizesDialog(numStudents, ui->idealTeamSizeBox->value(), this);

        //If user clicks OK, use these team sizes, otherwise revert to option 1, smaller team sizes
        int reply = win->exec();
        if(reply == QDialog::Accepted)
        {
            numTeams = win->numTeams;
            setTeamSizes(win->teamsizes);
        }
        else
        {
            // Set to smaller teams if cancelled
            bool oldState = ui->teamSizeBox->blockSignals(true);
            ui->teamSizeBox->setCurrentIndex(0);
            numTeams = teamingOptions.smallerTeamsNumTeams;
            setTeamSizes(teamingOptions.smallerTeamsSizes);
            ui->teamSizeBox->blockSignals(oldState);
        }

        delete win;
    }
    else if(index == 0)
    {
        // Smaller teams desired
        numTeams = teamingOptions.smallerTeamsNumTeams;
        setTeamSizes(teamingOptions.smallerTeamsSizes);
    }
    else if (index == 1)
    {
        // Larger teams desired
        numTeams = teamingOptions.largerTeamsNumTeams;
        setTeamSizes(teamingOptions.largerTeamsSizes);
    }
}


void gruepr::on_letsDoItButton_clicked()
{
    if(dataOptions.URMIncluded && teamingOptions.isolatedURMPrevented && teamingOptions.URMResponsesConsideredUR.isEmpty())
    {
        int resp = QMessageBox::warning(this, tr("gruepr"),
                                       tr("You have selected to prevented isolated URM students,\n"
                                          "however none of the race/ethnicity response values\n"
                                          "have been selected to be considered as underrepresented.\n\n"
                                          "Click OK to continue or Cancel to go back and select URM responses."),
                                       QMessageBox::Ok | QMessageBox::Cancel, QMessageBox::Cancel);
        if(resp == QMessageBox::Cancel)
        {
            ui->URMResponsesButton->setFocus();
            return;
        }
    }
    // Update UI
    ui->sectionSelectionBox->setEnabled(false);
    ui->teamSizeBox->setEnabled(false);
    ui->label_10->setEnabled(false);
    ui->idealTeamSizeBox->setEnabled(false);
    ui->loadSurveyFileButton->setEnabled(false);
    ui->saveTeamsButton->setEnabled(false);
    ui->printTeamsButton->setEnabled(false);
    ui->actionSave_Teams->setEnabled(false);
    ui->actionPrint_Teams->setEnabled(false);
    ui->letsDoItButton->setEnabled(false);
    ui->actionCreate_Teams->setEnabled(false);
    teamDataTree->setEnabled(false);

    // Normalize all score factor weights using norm factor = number of factors / total weights of all factors
    realNumScoringFactors = dataOptions.numAttributes + (dataOptions.dayNames.isEmpty()? 0 : 1);
    float normFactor = (float(realNumScoringFactors)) /
                       (std::accumulate(teamingOptions.attributeWeights, teamingOptions.attributeWeights + dataOptions.numAttributes, float(0.0)) +
                        (dataOptions.dayNames.isEmpty()? 0 : teamingOptions.scheduleWeight));
    for(int attribute = 0; attribute < dataOptions.numAttributes; attribute++)
    {
        realAttributeWeights[attribute] = teamingOptions.attributeWeights[attribute] * normFactor;
    }
    realScheduleWeight = (dataOptions.dayNames.isEmpty()? 0 : teamingOptions.scheduleWeight) * normFactor;

#ifdef Q_OS_WIN32
    // Set up to show progess on windows taskbar
    taskbarButton = new QWinTaskbarButton(this);
    taskbarButton->setWindow(windowHandle());
    taskbarProgress = taskbarButton->progress();
    taskbarProgress->show();
    taskbarProgress->setMaximum(0);
#endif

    // Create progress display plot
    progressChart = new BoxWhiskerPlot("", "Generation", "Scores");
    auto *chartView = new QtCharts::QChartView(progressChart);
    chartView->setRenderHint(QPainter::Antialiasing);

    // Create window to display progress, and connect the stop optimization button in the window to the actual stopping of the optimization thread
    progressWindow = new progressDialog("", chartView, this);
    progressWindow->show();
    connect(progressWindow, &progressDialog::letsStop, this, [this] {QApplication::setOverrideCursor(QCursor(Qt::BusyCursor));
                                                                     connect(this, &gruepr::turnOffBusyCursor, this, &QApplication::restoreOverrideCursor);
                                                                     optimizationStoppedmutex.lock();
                                                                     optimizationStopped = true;
                                                                     optimizationStoppedmutex.unlock();});

    // Get the IDs of students from desired section and change numStudents accordingly
    int numStudentsInSection = 0;
    studentIDs = new int[dataOptions.numStudentsInSystem];
    for(int recordNum = 0; recordNum < dataOptions.numStudentsInSystem; recordNum++)
    {
        if(ui->sectionSelectionBox->currentIndex() == 0 || ui->sectionSelectionBox->currentText() == student[recordNum].section)
        {
            studentIDs[numStudentsInSection] = student[recordNum].ID;
            numStudentsInSection++;
        }
    }
    numStudents = numStudentsInSection;

    // Set up the flag to allow a stoppage and set up futureWatcher to know when results are available
    optimizationStopped = false;
    future = QtConcurrent::run(this, &gruepr::optimizeTeams, studentIDs);       // spin optimization off into a separate thread
    futureWatcher.setFuture(future);                                // connect the watcher to get notified when optimization completes
}


void gruepr::updateOptimizationProgress(const QVector<float> &allScores, const int *orderedIndex, int generation, float scoreStability)
{
    if((generation % (progressChart->plotFrequency)) == 0)
    {
        progressChart->loadNextVals(allScores, orderedIndex);
    }

    if(generation > maxGenerations)
    {
        progressWindow->setText(tr("We have reached ") + QString::number(maxGenerations) + tr(" generations."),
                                generation, *std::max_element(allScores.constBegin(), allScores.constEnd()), true);
        progressWindow->highlightStopButton();
    }
    else if( (generation >= minGenerations) && (scoreStability > minScoreStability) )
    {
        progressWindow->setText(tr("Score appears to be stable."), generation, *std::max_element(allScores.constBegin(), allScores.constEnd()), true);
        progressWindow->highlightStopButton();
    }
    else
    {
        progressWindow->setText(tr("Optimization in progress."), generation, *std::max_element(allScores.constBegin(), allScores.constEnd()), false);
    }

#ifdef Q_OS_WIN32
    if(generation >= generationsOfStability)
    {
        taskbarProgress->setMaximum(100);
        taskbarProgress->setValue((scoreStability<100)? static_cast<int>(scoreStability) : 100);
    }
#endif
 }


void gruepr::optimizationComplete()
{
    //alert
    QApplication::beep();
    QApplication::alert(this);

    // update UI
    ui->sectionSelectionBox->setEnabled(ui->sectionSelectionBox->count() > 1);
    ui->label_2->setEnabled(ui->sectionSelectionBox->count() > 1);
    ui->label_22->setEnabled(ui->sectionSelectionBox->count() > 1);
    ui->teamSizeBox->setEnabled(true);
    ui->label_10->setEnabled(true);
    ui->idealTeamSizeBox->setEnabled(true);
    ui->loadSurveyFileButton->setEnabled(true);
    ui->dataDisplayTabWidget->setCurrentIndex(1);
    ui->saveTeamsButton->setEnabled(true);
    ui->printTeamsButton->setEnabled(true);
    ui->actionSave_Teams->setEnabled(true);
    ui->actionPrint_Teams->setEnabled(true);
    ui->letsDoItButton->setEnabled(true);
    ui->actionCreate_Teams->setEnabled(true);
    ui->teamDataLayout->setEnabled(true);
    teamDataTree->setEnabled(true);
    teamDataTree->setHeaderHidden(false);
    teamDataTree->collapseAll();
    ui->expandAllButton->setEnabled(true);
    ui->collapseAllButton->setEnabled(true);
    ui->label_14->setEnabled(true);
    ui->label_23->setEnabled(true);
    ui->teamNamesComboBox->setEnabled(true);
    bool signalsCurrentlyBlocked = ui->teamNamesComboBox->signalsBlocked();    // reset teamnames box to arabic numerals (without signaling the change)
    ui->teamNamesComboBox->blockSignals(true);
    ui->teamNamesComboBox->setCurrentIndex(0);
    ui->teamNamesComboBox->blockSignals(signalsCurrentlyBlocked);
#ifdef Q_OS_WIN32
    taskbarProgress->hide();
#endif
    delete progressChart;
    delete progressWindow;

    // free memory used to save array of IDs of students being teamed
    delete[] studentIDs;

    // Get the results
    QList<int> bestTeamSet = future.result();

    // Load students into teams
    int ID = 0;
    for(int team = 0; team < numTeams; team++)
    {
        teams[team].studentIDs.clear();
        for(int stud = 0; stud < teams[team].size; stud++)
        {
            teams[team].studentIDs << bestTeamSet.at(ID);
            ID++;
        }
        //sort teammates within a team alphabetically by lastname,firstname
        std::sort(teams[team].studentIDs.begin(), teams[team].studentIDs.end(),
                  [this] (const int a, const int b) {return ( (student[a].lastname + student[a].firstname) < (student[b].lastname + student[b].firstname) );});
    }

    // Get scores and other student info loaded
    refreshTeamInfo();

    // Sort teams by student name and set default teamnames
    std::sort(teams, teams+numTeams, [this](const TeamInfo &a, const TeamInfo &b) {return ( (student[a.studentIDs.at(0)].lastname + student[a.studentIDs.at(0)].firstname) <
                                                                                            (student[b.studentIDs.at(0)].lastname + student[b.studentIDs.at(0)].firstname) );});
    for(int team = 0; team < numTeams; team++)
    {
        teams[team].name = QString::number(team+1);
    }
    refreshTeamToolTips();

    // Display the results
    resetTeamDisplay();
    refreshTeamDisplay();

    // Sort by score and load initial order into currentSort column
    teamDataTree->sortByColumn(0, Qt::AscendingOrder);
    teamDataTree->headerItem()->setIcon(0, QIcon(":/icons/blank_arrow.png"));
    for(int team = 0; team < numTeams; team++)
    {
        teamDataTree->topLevelItem(team)->setData(teamDataTree->columnCount()-1, TeamInfoSort, team);
        teamDataTree->topLevelItem(team)->setData(teamDataTree->columnCount()-1, TeamInfoDisplay, QString::number(team));
        teamDataTree->topLevelItem(team)->setText(teamDataTree->columnCount()-1, QString::number(team));
    }
}


void gruepr::on_expandAllButton_clicked()
{
    teamDataTree->expandAll();
}


void gruepr::on_collapseAllButton_clicked()
{
    teamDataTree->collapseAll();
}


void gruepr::on_teamNamesComboBox_activated(int index)
{
    static int prevIndex = 0;   // hold on to previous index, so we can go back to it if cancelling custom team name dialog box
    QStringList teamNameLists = QString(listOfTeamNames).split(";");

    // Get team numbers in the order that they are currently displayed/sorted
    QList<int> teamDisplayNum;
    for(int row = 0; row < numTeams; row++)
    {
        int team = 0;
        while(teamDataTree->topLevelItem(row)->data(teamDataTree->columnCount()-1, TeamInfoSort).toInt() != team)
        {
            team++;
        }
        teamDisplayNum << teamDataTree->topLevelItem(row)->data(0, TeamNumber).toInt();
    }

    if(teamDataTree->sortColumn() == 0)
    {
        teamDataTree->sortByColumn(teamDataTree->columnCount()-1, Qt::AscendingOrder);
        teamDataTree->headerItem()->setIcon(0, QIcon(QIcon(":/icons/updown_arrow.png")));
    }

    // Set team names to:
    if(index == 0)
    {
        // arabic numbers
        for(int team = 0; team < numTeams; team++)
        {
            teams[teamDisplayNum.at(team)].name = QString::number(team+1);
        }
        prevIndex = 0;
    }
    else if(index == 1)
    {
        // roman numerals
        QString M[] = {"","M","MM","MMM"};
        QString C[] = {"","C","CC","CCC","CD","D","DC","DCC","DCCC","CM"};
        QString X[] = {"","X","XX","XXX","XL","L","LX","LXX","LXXX","XC"};
        QString I[] = {"","I","II","III","IV","V","VI","VII","VIII","IX"};
        for(int team = 0; team < numTeams; team++)
        {
            teams[teamDisplayNum.at(team)].name = M[(team+1)/1000]+C[((team+1)%1000)/100]+X[((team+1)%100)/10]+I[((team+1)%10)];
        }
        prevIndex = 1;
    }
    else if(index == 2)
    {
        // hexadecimal numbers
        for(int team = 0; team < numTeams; team++)
        {
            teams[teamDisplayNum.at(team)].name = QString::number(team, 16).toUpper();
        }
        prevIndex = 2;
    }
    else if(index < teamNameLists.size())
    {
        // Using one of the listed team names (given in gruepr_structs_and_consts.h)
        // Cycle through list as often as needed, adding a repetition every time through the list
        for(int team = 0; team < numTeams; team++)
        {
            QStringList teamNames = teamNameLists.at(index).split((","));
            teams[teamDisplayNum.at(team)].name = (teamNames[team%(teamNames.size())]+" ").repeated((team/teamNames.size())+1).trimmed();
        }
        prevIndex = index;
    }
    else if(ui->teamNamesComboBox->currentText() == tr("Current names"))
    {
        // Keeping the current custom names
    }
    else
    {
        // Open specialized dialog box to collect teamnames
        QStringList teamNames;
        for(int team = 0; team < numTeams; team++)
        {
            teamNames << teams[teamDisplayNum.at(team)].name;
        }
        auto *window = new customTeamnamesDialog(numTeams, teamNames, this);

        // If user clicks OK, use these team names, otherwise revert to previous option
        int reply = window->exec();
        if(reply == QDialog::Accepted)
        {
            for(int team = 0; team < numTeams; team++)
            {
                teams[teamDisplayNum.at(team)].name = (window->teamName[team].text().isEmpty()? QString::number(team+1) : window->teamName[team].text());
            }
            prevIndex = teamNameLists.size();
            bool currentValue = ui->teamNamesComboBox->blockSignals(true);
            ui->teamNamesComboBox->setCurrentIndex(prevIndex);
            ui->teamNamesComboBox->setItemText(teamNameLists.size(), tr("Current names"));
            ui->teamNamesComboBox->removeItem(teamNameLists.size()+1);
            ui->teamNamesComboBox->addItem(tr("Custom names"));
            ui->teamNamesComboBox->blockSignals(currentValue);
        }
        else
        {
            bool currentValue = ui->teamNamesComboBox->blockSignals(true);
            ui->teamNamesComboBox->setCurrentIndex(prevIndex);
            ui->teamNamesComboBox->blockSignals(currentValue);
        }

        delete window;
    }

    // Put list of options back to just built-ins plus "Custom names"
    if(ui->teamNamesComboBox->currentIndex() < teamNameLists.size())
    {
        ui->teamNamesComboBox->removeItem(teamNameLists.size()+1);
        ui->teamNamesComboBox->removeItem(teamNameLists.size());
        ui->teamNamesComboBox->addItem(tr("Custom names"));
    }

    // Update team names in table and other data
    refreshTeamToolTips();
    for(int team = 0; team < numTeams; team++)
    {
        parentItem[team]->setText(0, tr("Team ") + teams[team].name);
        parentItem[team]->setTextAlignment(0, Qt::AlignLeft | Qt::AlignVCenter);
        parentItem[team]->setData(0, TeamInfoDisplay, tr("Team ") + teams[team].name);
        for(int column = 0; column < teamDataTree->columnCount()-1; column++)
        {
            parentItem[team]->setToolTip(column, teams[team].tooltip);
        }
    }
    teamDataTree->resizeColumnToContents(0);
}


void gruepr::on_saveTeamsButton_clicked()
{
    createFileContents();
    const int previewLength = 1000;
    QStringList previews = {studentsFileContents.left(previewLength) + "...",
                            instructorsFileContents.mid(instructorsFileContents.indexOf("\n\n\nteam ", 0, Qt::CaseInsensitive)+3, previewLength) + "...",
                            spreadsheetFileContents.left(previewLength) + "..."};

    //Open specialized dialog box to choose which file(s) to save
    whichFilesDialog *window = new whichFilesDialog(whichFilesDialog::save, previews, this);
    int result = window->exec();

    if(result == QDialogButtonBox::Ok && (window->instructorFiletxt->isChecked() || window->studentFiletxt->isChecked() || window->spreadsheetFiletxt->isChecked()))
    {
        //save to text files
        QString baseFileName = QFileDialog::getSaveFileName(this, tr("Choose a location and base filename for the text file(s)"), "", tr("Text File (*.txt);;All Files (*)"));
        if ( !(baseFileName.isEmpty()) )
        {
            bool problemSaving = false;
            if(window->instructorFiletxt->isChecked())
            {
                QFile instructorsFile(QFileInfo(baseFileName).path() + "/" + QFileInfo(baseFileName).completeBaseName() + "_instructor." + QFileInfo(baseFileName).suffix());
                if(instructorsFile.open(QIODevice::WriteOnly | QIODevice::Text))
                {
                    QTextStream out(&instructorsFile);
                    out << instructorsFileContents;
                    instructorsFile.close();
                }
                else
                {
                    problemSaving = true;
                }
            }
            if(window->studentFiletxt->isChecked())
            {
                QFile studentsFile(QFileInfo(baseFileName).path() + "/" + QFileInfo(baseFileName).completeBaseName() + "_student." + QFileInfo(baseFileName).suffix());
                if(studentsFile.open(QIODevice::WriteOnly | QIODevice::Text))
                {
                    QTextStream out(&studentsFile);
                    out << studentsFileContents;
                    studentsFile.close();
                }
                else
                {
                    problemSaving = true;
                }
            }
            if(window->spreadsheetFiletxt->isChecked())
            {
                QFile spreadsheetFile(QFileInfo(baseFileName).path() + "/" + QFileInfo(baseFileName).completeBaseName() + "_spreadsheet." + QFileInfo(baseFileName).suffix());
                if(spreadsheetFile.open(QIODevice::WriteOnly | QIODevice::Text))
                {
                    QTextStream out(&spreadsheetFile);
                    out << spreadsheetFileContents;
                    spreadsheetFile.close();
                }
                else
                {
                    problemSaving = true;
                }
            }
            if(problemSaving)
            {
                QMessageBox::critical(this, tr("No Files Saved"), tr("No files were saved.\nThere was an issue writing the files."));
            }
            else
            {
                setWindowModified(false);
            }
        }
    }
    if(result == QDialogButtonBox::Ok && (window->instructorFilepdf->isChecked() || window->studentFilepdf->isChecked()))
    {
        //save as formatted pdf files
        printFiles(window->instructorFilepdf->isChecked(), window->studentFilepdf->isChecked(), false, true);
    }
    delete window;
}


void gruepr::on_printTeamsButton_clicked()
{
    createFileContents();
    const int previewLength = 1000;
    QStringList previews = {studentsFileContents.left(previewLength) + "...",
                            instructorsFileContents.mid(instructorsFileContents.indexOf("\n\n\nteam ", 0, Qt::CaseInsensitive)+3, previewLength) + "...",
                            spreadsheetFileContents.left(previewLength) + "..."};

    //Open specialized dialog box to choose which file(s) to print
    whichFilesDialog *window = new whichFilesDialog(whichFilesDialog::print, previews, this);
    int result = window->exec();

    if(result == QDialogButtonBox::Ok && (window->instructorFilepdf->isChecked() || window->studentFilepdf->isChecked() || window->spreadsheetFiletxt->isChecked()))
    {
        printFiles(window->instructorFilepdf->isChecked(), window->studentFilepdf->isChecked(), window->spreadsheetFiletxt->isChecked(), false);
    }
    delete window;
}


void gruepr::swapTeammates(int studentAteam, int studentAID, int studentBteam, int studentBID)
{
    if(studentAID == studentBID)
    {
        return;
    }

    //maintain current sort order
    teamDataTree->sortByColumn(teamDataTree->columnCount()-1, Qt::AscendingOrder);

    if(studentAteam == studentBteam)
    {
#if QT_VERSION >= QT_VERSION_CHECK(5,13,0)
        teams[studentAteam].studentIDs.swapItemsAt(teams[studentAteam].studentIDs.indexOf(studentAID), teams[studentBteam].studentIDs.indexOf(studentBID));
#else
        teams[studentAteam].studentIDs.swap(teams[studentAteam].studentIDs.indexOf(studentAID), teams[studentBteam].studentIDs.indexOf(studentBID));
#endif
        refreshTeamInfo(QList<int>({studentAteam}));
        refreshTeamToolTips(QList<int>({studentAteam}));
        refreshTeamDisplay(QList<int>({studentAteam}));
    }
    else
    {
        teams[studentAteam].studentIDs.replace(teams[studentAteam].studentIDs.indexOf(studentAID), studentBID);
        teams[studentBteam].studentIDs.replace(teams[studentBteam].studentIDs.indexOf(studentBID), studentAID);
        refreshTeamInfo(QList<int>({studentAteam, studentBteam}));
        refreshTeamToolTips(QList<int>({studentAteam, studentBteam}));
        refreshTeamDisplay(QList<int>({studentAteam, studentBteam}));
    }
}


void gruepr::swapTeams(int teamA, int teamB)
{
    if(teamA == teamB)
    {
        return;
    }

    // find the teamA and teamB top level items in teamDataTree
    int teamARow=0, teamBRow=0;
    for(int row = 0; row < numTeams; row++)
    {
        if(teamDataTree->topLevelItem(row)->data(0, TeamNumber).toInt() == teamA)
        {
            teamARow = row;
        }
        else if(teamDataTree->topLevelItem(row)->data(0, TeamNumber).toInt() == teamB)
        {
            teamBRow = row;
        }
    }

    int teamASortOrder = teamDataTree->topLevelItem(teamARow)->data(teamDataTree->columnCount()-1, TeamInfoSort).toInt();
    int teamBSortOrder = teamDataTree->topLevelItem(teamBRow)->data(teamDataTree->columnCount()-1, TeamInfoSort).toInt();
    teamDataTree->topLevelItem(teamARow)->setData(teamDataTree->columnCount()-1, TeamInfoSort, teamBSortOrder);
    teamDataTree->topLevelItem(teamARow)->setData(teamDataTree->columnCount()-1, TeamInfoDisplay, QString::number(teamBSortOrder));
    teamDataTree->topLevelItem(teamARow)->setText(teamDataTree->columnCount()-1, QString::number(teamBSortOrder));
    teamDataTree->topLevelItem(teamBRow)->setData(teamDataTree->columnCount()-1, TeamInfoSort, teamASortOrder);
    teamDataTree->topLevelItem(teamBRow)->setData(teamDataTree->columnCount()-1, TeamInfoDisplay, QString::number(teamASortOrder));
    teamDataTree->topLevelItem(teamBRow)->setText(teamDataTree->columnCount()-1, QString::number(teamASortOrder));

    std::swap(expanded[teamA], expanded[teamB]);

    teamDataTree->sortByColumn(teamDataTree->columnCount()-1, Qt::AscendingOrder);

    refreshTeamDisplay(QList<int>({teamA, teamB}));
    for(int team = 0; team < numTeams; team++)
    {
        teamDataTree->topLevelItem(team)->setData(teamDataTree->columnCount()-1, TeamInfoSort, team);
        teamDataTree->topLevelItem(team)->setData(teamDataTree->columnCount()-1, TeamInfoDisplay, QString::number(team));
        teamDataTree->topLevelItem(team)->setText(teamDataTree->columnCount()-1, QString::number(team));
    }
}


void gruepr::reorderedTeams()
{
    QCoreApplication::processEvents();  // make sure the sorting happens first
    for(int team = 0; team < numTeams; team++)
    {
        teamDataTree->topLevelItem(team)->setData(teamDataTree->columnCount()-1, TeamInfoSort, team);
        teamDataTree->topLevelItem(team)->setData(teamDataTree->columnCount()-1, TeamInfoDisplay, QString::number(team));
        teamDataTree->topLevelItem(team)->setText(teamDataTree->columnCount()-1, QString::number(team));
    }
}


void gruepr::settingsWindow()
{
}


void gruepr::helpWindow()
{
    QFile helpFile(":/help.html");
    if (!helpFile.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        return;
    }
    QDialog helpWindow(this);
    helpWindow.setWindowFlags(Qt::Dialog | Qt::WindowTitleHint | Qt::WindowCloseButtonHint);
    helpWindow.setSizeGripEnabled(true);
    helpWindow.setWindowTitle("Help");
    QGridLayout theGrid(&helpWindow);
    QTextBrowser helpContents(&helpWindow);
    helpContents.setHtml(tr("<h1 style=\"font-family:'Oxygen Mono';\">gruepr " GRUEPR_VERSION_NUMBER "</h1>"
                            "<p>Copyright &copy; " GRUEPR_COPYRIGHT_YEAR
                            "<p>Joshua Hertz <a href = mailto:gruepr@gmail.com>gruepr@gmail.com</a>"
                            "<p>Project homepage: <a href = http://bit.ly/Gruepr>http://bit.ly/Gruepr</a>"));
    helpContents.append(helpFile.readAll());
    helpFile.close();
    helpContents.setOpenExternalLinks(true);
    helpContents.setFrameShape(QFrame::NoFrame);
    theGrid.addWidget(&helpContents, 0, 0, -1, -1);
    helpWindow.resize(600,600);
    helpWindow.exec();
}


void gruepr::aboutWindow()
{
    QSettings savedSettings;
    QString registeredUser = savedSettings.value("registeredUser", "").toString();
    QString user = registeredUser.isEmpty()? tr("UNREGISTERED") : (tr("registered to ") + registeredUser);
    QMessageBox::about(this, tr("About gruepr"),
                       tr("<h1 style=\"font-family:'Oxygen Mono';\">gruepr " GRUEPR_VERSION_NUMBER "</h1>"
                          "<p>Copyright &copy; " GRUEPR_COPYRIGHT_YEAR
                          "<br>Joshua Hertz<br><a href = mailto:gruepr@gmail.com>gruepr@gmail.com</a>"
                          "<p>This copy of gruepr is ") + user + tr("."
                          "<p>gruepr is an open source project. The source code is freely available at"
                          "<br>the project homepage: <a href = http://bit.ly/Gruepr>http://bit.ly/Gruepr</a>."
                          "<p>gruepr incorporates:"
                              "<ul><li>Code libraries from <a href = http://qt.io>Qt, v 5.12 or 5.13</a>, released under the GNU Lesser General Public License version 3</li>"
                              "<li>Icons from <a href = https://icons8.com>Icons8</a>, released under Creative Commons license \"Attribution-NoDerivs 3.0 Unported\"</li>"
                              "<li><span style=\"font-family:'Oxygen Mono';\">The font <a href = https://www.fontsquirrel.com/fonts/oxygen-mono>"
                                                                    "Oxygen Mono</a>, Copyright &copy; 2012, Vernon Adams (vern@newtypography.co.uk),"
                                                                    " released under SIL OPEN FONT LICENSE V1.1.</span></li>"
                              "<li>A photo of a grouper, courtesy Rich Whalen</li></ul>"
                          "<h3>Disclaimer</h3>"
                          "<p>This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or "
                          "FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details."
                          "<p>This program is free software: you can redistribute it and/or modify it under the terms of the <a href = https://www.gnu.org/licenses/gpl.html>"
                          "GNU General Public License</a> as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version."));
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////
// Functions
/////////////////////////////////////////////////////////////////////////////////////////////////////////


//////////////////
//Load window geometry and default teaming options saved from previous run. If non-existant, load app defaults.
//////////////////
void gruepr::loadDefaultSettings()
{
    QSettings savedSettings;

    //Restore window geometry
    restoreGeometry(savedSettings.value("windowGeometry").toByteArray());

    //Restore last data file folder location
    dataOptions.dataFile.setFile(savedSettings.value("dataFileLocation", "").toString());

    //Restore teaming options
    ui->idealTeamSizeBox->setValue(savedSettings.value("idealTeamSize", 4).toInt());
    on_idealTeamSizeBox_valueChanged(ui->idealTeamSizeBox->value());        // load new team sizes in teamingOptions and in ui selection box
    teamingOptions.isolatedWomenPrevented = savedSettings.value("isolatedWomenPrevented", false).toBool();
    ui->isolatedWomenCheckBox->setChecked(teamingOptions.isolatedWomenPrevented);
    teamingOptions.isolatedMenPrevented = savedSettings.value("isolatedMenPrevented", false).toBool();
    ui->isolatedMenCheckBox->setChecked(teamingOptions.isolatedMenPrevented);
    teamingOptions.singleGenderPrevented = savedSettings.value("singleGenderPrevented", false).toBool();
    ui->mixedGenderCheckBox->setChecked(teamingOptions.singleGenderPrevented);
    teamingOptions.isolatedURMPrevented = savedSettings.value("isolatedURMPrevented", false).toBool();
    ui->isolatedURMCheckBox->blockSignals(true);    // prevent select URM identities box from immediately opening
    ui->isolatedURMCheckBox->setChecked(teamingOptions.isolatedURMPrevented);
    ui->isolatedURMCheckBox->blockSignals(false);
    teamingOptions.minTimeBlocksOverlap = savedSettings.value("minTimeBlocksOverlap", 4).toInt();
    ui->minMeetingTimes->setValue(teamingOptions.minTimeBlocksOverlap);
    teamingOptions.desiredTimeBlocksOverlap = savedSettings.value("desiredTimeBlocksOverlap", 8).toInt();
    ui->desiredMeetingTimes->setValue(teamingOptions.desiredTimeBlocksOverlap);
    teamingOptions.meetingBlockSize = savedSettings.value("meetingBlockSize", 1).toInt();
    ui->meetingLength->setCurrentIndex(teamingOptions.meetingBlockSize-1);
    teamingOptions.scheduleWeight = savedSettings.value("scheduleWeight", 4).toFloat();
    ui->scheduleWeight->setValue(double(teamingOptions.scheduleWeight));
    savedSettings.beginReadArray("Attributes");
    for (int attribNum = 0; attribNum < maxAttributes; ++attribNum)
    {
        savedSettings.setArrayIndex(attribNum);
        teamingOptions.desireHomogeneous[attribNum] = savedSettings.value("desireHomogeneous", false).toBool();
        teamingOptions.attributeWeights[attribNum] = savedSettings.value("Weight", 1).toFloat();
        int numIncompats = savedSettings.beginReadArray("incompatibleResponses");
        for(int incompResp = 0; incompResp < numIncompats; incompResp++)
        {
            savedSettings.setArrayIndex(incompResp);
            QStringList incompats = savedSettings.value("incompatibleResponses", "").toString().split(',');
            teamingOptions.incompatibleAttributeValues[attribNum] << QPair<int,int>(incompats.at(0).toInt(),incompats.at(1).toInt());
        }
        savedSettings.endArray();
    }
    savedSettings.endArray();
    if(ui->attributeScrollBar->value() == 0)
    {
        on_attributeScrollBar_valueChanged(0);      // displays the correct attribute weight, homogeneity, text in case scrollbar is already at 0
    }
    else
    {
        ui->attributeScrollBar->setValue(0);
    }
    teamingOptions.numberRequestedTeammatesGiven = savedSettings.value("requestedTeammateNumber", 1).toInt();
    ui->requestedTeammateNumberBox->setValue(teamingOptions.numberRequestedTeammatesGiven);
}


//////////////////
// Set the "official" team sizes using an array of different sizes or a single, constant size
//////////////////
void gruepr::setTeamSizes(const int teamSizes[])
{
    delete[] teams;
    teams = new TeamInfo[numTeams];
    for(int team = 0; team < numTeams; team++)	// run through every team
    {
        teams[team].size = teamSizes[team];
    }
}
void gruepr::setTeamSizes(const int singleSize)
{
    delete[] teams;
    teams = new TeamInfo[numTeams];
    for(int team = 0; team < numTeams; team++)	// run through every team
    {
        teams[team].size = singleSize;
    }
}


//////////////////
// Read the survey datafile, setting the data options and loading all of the student records, returning true if successful and false if file is invalid
//////////////////
bool gruepr::loadSurveyData(const QString &fileName)
{
    QFile inputFile(fileName);
    inputFile.open(QIODevice::ReadOnly);
    QTextStream in(&inputFile);

    // Read the header row to determine what data is included
    QStringList fields = ReadCSVLine(in.readLine());
    int TotNumQuestions = fields.size();
    if(fields.size() < 4)       // need at least timestamp, first name, last name, email address
    {
        QMessageBox::critical(this, tr("File error."),
                             tr("This file is empty or there is an error in its format."), QMessageBox::Ok);
        inputFile.close();
        return false;
    }

    // Read the optional gender/URM/attribute questions
    int fieldnum = 4;           // skipping past required fields: timestamp(0), first name(1), last name(2), email address(3)
    QString field = fields.at(fieldnum).toUtf8();
    // See if gender data is included
    if(field.contains(tr("gender"), Qt::CaseInsensitive))
    {
        dataOptions.genderIncluded = true;
        fieldnum++;
        field = fields.at(fieldnum).toUtf8();
    }
    else
    {
        dataOptions.genderIncluded = false;
    }

    // See if URM data is included
    if(field.contains(tr("minority"), Qt::CaseInsensitive) || field.contains(tr("ethnic"), Qt::CaseInsensitive))
    {
        dataOptions.URMIncluded = true;
        fieldnum++;
        field = fields.at(fieldnum).toUtf8();
    }
    else
    {
        dataOptions.URMIncluded = false;
    }

    // Count the number of attributes by counting number of questions from here until one includes "check...times," "In which section are you enrolled", or end of the line is reached.
    // Save these attribute question texts, if any, into string list.
    dataOptions.numAttributes = 0;                              // how many skill/attitude rankings are there?
    while(!(field.contains(QRegularExpression(".*(check).+(times).+", QRegularExpression::CaseInsensitiveOption)))
                    && !(field.contains("in which section are you enrolled", Qt::CaseInsensitive))
                    && (fieldnum < TotNumQuestions) )
    {
        dataOptions.attributeQuestionText << field;
        dataOptions.numAttributes++;
        fieldnum++;
        if(fieldnum < TotNumQuestions)
        {
            field = fields.at(fieldnum).toUtf8();				// move on to next field
        }
    }

    // Count the number of days in the schedule by counting number of questions that includes "Check...times".
    // Save the day names and save which fields they are for use later in getting the time names
    QVector<int> scheduleFields;
    while(field.contains(QRegularExpression(".*(check).+(times).+", QRegularExpression::CaseInsensitiveOption)) && fieldnum < TotNumQuestions)
    {
        if(field.contains(QRegularExpression(".+(free|available).+", QRegularExpression::CaseInsensitiveOption)))   // if even one field has this language, all are interpreted as free time
        {
            dataOptions.scheduleDataIsFreetime = true;
        }
        QRegularExpression dayNameFinder("\\[([^[]*)\\]");   // Day name should be in brackets at the end of the field (that's where Google Forms puts column titles in matrix questions)
        QRegularExpressionMatch dayName = dayNameFinder.match(field);
        if(dayName.hasMatch())
        {
            dataOptions.dayNames << dayName.captured(1);
        }
        else
        {
            dataOptions.dayNames << " " + QString::number(scheduleFields.size()+1) + " ";
        }
        scheduleFields << fieldnum;
        fieldnum++;
        if(fieldnum < TotNumQuestions)
        {
            field = fields.at(fieldnum).toUtf8();				// move on to next field
        }
    }

    // Look for any remaining questions
    if(TotNumQuestions > fieldnum)                                            // There is at least 1 additional field in header
    {
        field = fields.at(fieldnum).toUtf8();
        if(field.contains("in which section are you enrolled", Qt::CaseInsensitive))			// next field is a section question
        {
            fieldnum++;
            if(TotNumQuestions > fieldnum)                                    // if there are any more fields after section
            {
                dataOptions.sectionIncluded = true;
                dataOptions.notesIncluded = true;
            }
            else
            {
                dataOptions.sectionIncluded = true;
                dataOptions.notesIncluded = false;
            }
        }
        else
        {
            dataOptions.sectionIncluded = false;
            dataOptions.notesIncluded = true;
        }
    }
    else
    {
        dataOptions.notesIncluded = false;
        dataOptions.sectionIncluded = false;
    }

    // remember where we are and read one line of data
    qint64 endOfHeaderRow = in.pos();
    fields = ReadCSVLine(in.readLine(), TotNumQuestions);

    // no data after header row--file is invalid
    if(fields.isEmpty())
    {
        QMessageBox::critical(this, tr("Insufficient number of students."),
                             tr("There are no survey responses in this file."), QMessageBox::Ok);
        inputFile.close();
        return false;
    }

    // If there is schedule info, read through the schedule fields in all of the responses to compile a list of time names, save as dataOptions.TimeNames
    if(!dataOptions.dayNames.isEmpty())
    {
        QStringList allTimeNames;
        while(!fields.isEmpty())
        {
            for(auto i : qAsConst(scheduleFields))
            {
                allTimeNames << ReadCSVLine(QString(fields.at(i).toUtf8()).toLower().split(';').join(','));
            }
            fields = ReadCSVLine(in.readLine(), TotNumQuestions);
        }
        allTimeNames.removeDuplicates();
        allTimeNames.removeOne("");
        //sort allTimeNames smartly, using mapped string -> hour of day integer
        QStringList timeNamesStrings = QString(timeNames).split(",");
        std::sort(allTimeNames.begin(), allTimeNames.end(), [&timeNamesStrings](const QString &a, const QString &b) -> bool
                                                            {return timeMeanings[timeNamesStrings.indexOf(a)] < timeMeanings[timeNamesStrings.indexOf(b)];});
        dataOptions.timeNames = allTimeNames;
    }

    in.seek(endOfHeaderRow);    // put cursor back to end of header row

    // Having read the header row and determined time names, if any, read each remaining row as a student record
    numStudents = 0;    // counter for the number of records in the file; used to set the number of students to be teamed for the rest of the program
    fields = ReadCSVLine(in.readLine(), TotNumQuestions);
    while(!fields.isEmpty() && numStudents < maxStudents)
    {
        student[numStudents] = readOneRecordFromFile(fields);
        student[numStudents].ID = numStudents;
        numStudents++;
        fields = ReadCSVLine(in.readLine(), TotNumQuestions);
    }
    dataOptions.numStudentsInSystem = numStudents;

    // Set the attribute question options and numerical values for each student
    for(int attrib = 0; attrib < maxAttributes; attrib++)
    {
        // gather all unique attribute question responses, remove a blank response if it exists in a list with other responses, and then sort
        for(int ID = 0; ID < dataOptions.numStudentsInSystem; ID++)
        {
            if(!dataOptions.attributeQuestionResponses[attrib].contains(student[ID].attributeResponse[attrib]))
            {
                dataOptions.attributeQuestionResponses[attrib] << student[ID].attributeResponse[attrib];
            }
        }
        if(dataOptions.attributeQuestionResponses[attrib].size() > 1)
        {
            dataOptions.attributeQuestionResponses[attrib].removeAll(QString(""));
        }
        QCollator sortAlphanumerically;
        sortAlphanumerically.setNumericMode(true);
        sortAlphanumerically.setCaseSensitivity(Qt::CaseInsensitive);
        std::sort(dataOptions.attributeQuestionResponses[attrib].begin(), dataOptions.attributeQuestionResponses[attrib].end(), sortAlphanumerically);

        // if every response starts with an integer but not decimal, then it is ordered (numerical); if any response is missing this, then it is categorical
        dataOptions.attributeIsOrdered[attrib] = true;
        QRegularExpression startsWithDecimal("^\\d+\\.\\d+");   // leading decimal value: 1+ digits, decimal point, 1+ digits
        QRegularExpression startsWithInteger("^\\d+\\D");       // leading integer value: 1+ digits, 1 non-digits
        for(int response = 0; response < dataOptions.attributeQuestionResponses[attrib].size(); response++)
        {
            dataOptions.attributeIsOrdered[attrib] &= (startsWithInteger.match(dataOptions.attributeQuestionResponses[attrib].at(response)).hasMatch() &&
                                                       !startsWithDecimal.match(dataOptions.attributeQuestionResponses[attrib].at(response)).hasMatch());
        }

        if(dataOptions.attributeIsOrdered[attrib])
        {
            // ordered/numerical values. attribute scores will be based on number at the first and last response
            dataOptions.attributeMin[attrib] = startsWithInteger.match(dataOptions.attributeQuestionResponses[attrib].first()).captured().chopped(1).toInt();
            dataOptions.attributeMax[attrib] = startsWithInteger.match(dataOptions.attributeQuestionResponses[attrib].last()).captured().chopped(1).toInt();
            // set numerical value of students' attribute responses according to the number at the start of the response
            for(int ID = 0; ID < dataOptions.numStudentsInSystem; ID++)
            {
                if(!student[ID].attributeResponse[attrib].isEmpty())
                {
                    student[ID].attribute[attrib] = startsWithInteger.match(student[ID].attributeResponse[attrib]).captured().chopped(1).toInt();
                }
                else
                {
                    student[ID].attribute[attrib] = -1;
                }
            }
        }
        else
        {
            // categorical values. attribute scores will count up to the number of responses
            dataOptions.attributeMin[attrib] = 1;
            dataOptions.attributeMax[attrib] = dataOptions.attributeQuestionResponses[attrib].size();
            // set numerical value of students' attribute responses according to their place in the sorted list of responses
            for(int ID = 0; ID < dataOptions.numStudentsInSystem; ID++)
            {
                if(!student[ID].attributeResponse[attrib].isEmpty())
                {
                    student[ID].attribute[attrib] = dataOptions.attributeQuestionResponses[attrib].indexOf(student[ID].attributeResponse[attrib]) + 1;
                }
                else
                {
                    student[ID].attribute[attrib] = -1;
                }
            }
        }

    }

    // gather all unique URM question responses and sort
    for(int ID = 0; ID < dataOptions.numStudentsInSystem; ID++)
    {
        if(!dataOptions.URMResponses.contains(student[ID].URMResponse, Qt::CaseInsensitive))
        {
            dataOptions.URMResponses << student[ID].URMResponse;
        }
    }
    QCollator sortAlphanumerically;
    sortAlphanumerically.setNumericMode(true);
    sortAlphanumerically.setCaseSensitivity(Qt::CaseInsensitive);
    std::sort(dataOptions.URMResponses.begin(), dataOptions.URMResponses.end(), sortAlphanumerically);
    if(dataOptions.URMResponses.contains("--"))
    {
        // put the blank response option at the end of the list
        dataOptions.URMResponses.removeAll("--");
        dataOptions.URMResponses << "--";
    }

    if(numStudents == maxStudents)
    {
        QMessageBox::warning(this, tr("Reached maximum number of students."),
                             tr("The maximum number of students have been read from the file."
                                " This version of gruepr does not allow more than ") + QString(maxStudents) + tr("."), QMessageBox::Ok);
    }
    else if(numStudents < 4)
    {
        QMessageBox::critical(this, tr("Insufficient number of students."),
                             tr("There are not enough survey responses in the file."
                                " There must be at least 4 students for gruepr to work properly."), QMessageBox::Ok);
        //reset the data
        delete[] student;
        student = new StudentRecord[maxStudents];
        dataOptions.numStudentsInSystem = 0;
        dataOptions.numAttributes = 0;
        dataOptions.attributeQuestionText.clear();
        for(int attrib = 0; attrib < maxAttributes; attrib++)
        {
            dataOptions.attributeQuestionResponses[attrib].clear();
        }
        dataOptions.dayNames.clear();
        dataOptions.timeNames.clear();

        inputFile.close();
        return false;
    }

    inputFile.close();
    return true;
}


//////////////////
// Read one student's info from the survey datafile
//////////////////
StudentRecord gruepr::readOneRecordFromFile(const QStringList &fields)
{
    StudentRecord student;

    // fields are: 1) timestamp, 2) firstname, 3) lastname, 4) email, 5) gender, 5-6) URM, 5-15) attributes, 5-22) days in schedule, 5-23) section, 5-24) notes

    // first 4 fields: timestamp, first or preferred name, last name, email address
    int fieldnum = 0;
    student.surveyTimestamp = QDateTime::fromString(fields.at(fieldnum).left(fields.at(fieldnum).lastIndexOf(' ')), TIMESTAMP_FORMAT1); // format when downloaded direct from Form
    if(student.surveyTimestamp.isNull())
    {
        student.surveyTimestamp = QDateTime::fromString(fields.at(fieldnum).left(fields.at(fieldnum).lastIndexOf(' ')), TIMESTAMP_FORMAT2); // alt. format when downloaded direct from Form
        if(student.surveyTimestamp.isNull())
        {
            student.surveyTimestamp = QDateTime::fromString(fields.at(fieldnum), TIMESTAMP_FORMAT3);   // format when downloaded from Results Spreadsheet
            if(student.surveyTimestamp.isNull())
            {
                student.surveyTimestamp = QDateTime::fromString(fields.at(fieldnum), TIMESTAMP_FORMAT4);   // format when saving a csv edited in Excel
                if(student.surveyTimestamp.isNull())
                {
                    student.surveyTimestamp = QDateTime::fromString(fields.at(fieldnum), Qt::TextDate);
                    if(student.surveyTimestamp.isNull())
                    {
                        student.surveyTimestamp = QDateTime::fromString(fields.at(fieldnum), Qt::ISODate);
                        if(student.surveyTimestamp.isNull())
                        {
                            student.surveyTimestamp = QDateTime::fromString(fields.at(fieldnum), Qt::ISODateWithMs);
                        if(student.surveyTimestamp.isNull())
                        {
                            student.surveyTimestamp = QDateTime::fromString(fields.at(fieldnum), Qt::SystemLocaleShortDate);
                        if(student.surveyTimestamp.isNull())
                        {
                            student.surveyTimestamp = QDateTime::fromString(fields.at(fieldnum), Qt::SystemLocaleLongDate);
                        if(student.surveyTimestamp.isNull())
                        {
                            student.surveyTimestamp = QDateTime::fromString(fields.at(fieldnum), Qt::RFC2822Date);
                        if(student.surveyTimestamp.isNull())
                        {
                            student.surveyTimestamp = QDateTime::currentDateTime();
                        }
                        }
                        }
                        }
                        }
                    }
                }
            }
        }
    }

    fieldnum++;
    student.firstname = fields.at(fieldnum).toUtf8().trimmed();
    student.firstname[0] = student.firstname[0].toUpper();

    fieldnum++;
    student.lastname = fields.at(fieldnum).toUtf8().trimmed();
    student.lastname[0] = student.lastname[0].toUpper();

    fieldnum++;
    student.email = fields.at(fieldnum).toUtf8().trimmed();

    // optional 5th field in line; might be the gender
    fieldnum++;
    if(dataOptions.genderIncluded)
    {
        QString field = fields.at(fieldnum).toUtf8();
        if(field.contains(tr("woman"), Qt::CaseInsensitive))
        {
            student.gender = StudentRecord::woman;
        }
        else if(field.contains(tr("man"), Qt::CaseInsensitive))
        {
            student.gender = StudentRecord::man;
        }
        else
        {
            student.gender = StudentRecord::neither;
        }
        fieldnum++;
    }
    else
    {
        student.gender = StudentRecord::neither;
    }

    // optional next field in line; might be underrpresented minority status
    if(dataOptions.URMIncluded)
    {
        QString field = fields.at(fieldnum).toUtf8().toLower().simplified();
        if(field == "")
        {
            field = tr("--");
        }
        student.URMResponse = field;
        fieldnum++;
    }
    else
    {
        student.URM = false;
    }

    // optional next 9 fields in line; might be the attributes
    for(int attribute = 0; attribute < dataOptions.numAttributes; attribute++)
    {
        QString field = fields.at(fieldnum).toUtf8();
        student.attributeResponse[attribute] = field;
        fieldnum++;
    }

    // next 0-7 fields; might be the schedule
    for(int day = 0; day < dataOptions.dayNames.size(); day++)
    {
        QString field = fields.at(fieldnum).toUtf8();
        for(int time = 0; time < dataOptions.timeNames.size(); time++)
        {
            student.unavailable[(day*dataOptions.timeNames.size())+time] = field.contains(dataOptions.timeNames.at(time).toUtf8(), Qt::CaseInsensitive);
            if(dataOptions.scheduleDataIsFreetime)
            {
                student.unavailable[(day*dataOptions.timeNames.size())+time] = !student.unavailable[(day*dataOptions.timeNames.size())+time];
            }
        }
        fieldnum++;
    }   
    if(!dataOptions.dayNames.isEmpty())
    {
        student.availabilityChart = tr("Availability:");
        student.availabilityChart += "<table style='padding: 0px 3px 0px 3px;'><tr><th></th>";
        for(int day = 0; day < dataOptions.dayNames.size(); day++)
        {
            student.availabilityChart += "<th>" + dataOptions.dayNames.at(day).toUtf8().left(3) + "</th>";   // using first 3 characters in day name as abbreviation
        }
        student.availabilityChart += "</tr>";
        for(int time = 0; time < dataOptions.timeNames.size(); time++)
        {
            student.availabilityChart += "<tr><th>" + dataOptions.timeNames.at(time).toUtf8() + "</th>";
            for(int day = 0; day < dataOptions.dayNames.size(); day++)
            {
                student.availabilityChart += QString(student.unavailable[(day*dataOptions.timeNames.size())+time]?
                                                  "<td align = center> </td>" : "<td align = center bgcolor='PaleGreen'><b>√</b></td>");
            }
            student.availabilityChart += "</tr>";
        }
        student.availabilityChart += "</table>";
    }
    student.ambiguousSchedule = (student.availabilityChart.count("√") == 0 || student.availabilityChart.count("√") == (dataOptions.dayNames.size() * dataOptions.timeNames.size()));

    // optional last fields; might be section and/or additional notes
    if(dataOptions.sectionIncluded)
    {
        student.section = fields.at(fieldnum).toUtf8().trimmed();
        if(student.section.startsWith("section",Qt::CaseInsensitive))
        {
            student.section = student.section.right(student.section.size()-7).trimmed();    //removing as redundant the word "section" if at the start of the section name
        }
        fieldnum++;
    }

    if(dataOptions.notesIncluded)
    {
        student.notes = fields.mid(fieldnum).join('\n').toUtf8().trimmed();     //all remaining fields
    }

    return student;
}


//////////////////
// Read one line from a CSV file, smartly handling commas within fields that are enclosed by quotation marks
//////////////////
QStringList gruepr::ReadCSVLine(const QString &line, int minFields)
{
    enum State {Normal, Quote} state = Normal;
    QStringList fields;
    QString value;

    for(int i = 0; i < line.size(); i++)
    {
        QChar current=line.at(i);

        // Normal state
        if (state == Normal)
        {
            // Comma
            if (current == ',')
            {
                // Save field
                fields.append(value.trimmed());
                value.clear();
            }

            // Double-quote
            else if (current == '"')
            {
                state = Quote;
                value += current;
            }

            // Other character
            else
            {
                value += current;
            }
        }

        // In-quote state
        else if (state == Quote)
        {
            // Another double-quote
            if (current == '"')
            {
                if (i < line.size())
                {
                    // A double double-quote?
                    if (i+1 < line.size() && line.at(i+1) == '"')
                    {
                        value += '"';

                        // Skip a second quote character in a row
                        i++;
                    }
                    else
                    {
                        state = Normal;
                        value += '"';
                    }
                }
            }

            // Other character
            else
            {
                value += current;
            }
        }
    }
    if (!value.isEmpty())
    {
        fields.append(value.trimmed());
    }

    // Quotes are left in until here; so when fields are trimmed, only whitespace outside of
    // quotes is removed.  The quotes are removed here.
    for (int i=0; i<fields.size(); ++i)
    {
        if(fields[i].length() >= 1)
        {
            if(fields[i].at(0) == '"')
            {
                fields[i] = fields[i].mid(1);
                if(fields[i].length() >= 1)
                {
                    if(fields[i].right(1) == '"')
                    {
                        fields[i] = fields[i].left(fields[i].length() - 1);
                    }
                }
            }
        }
    }

    if(minFields == -1)      // default value of -1 means just return however many fields are found
    {
        return fields;
    }

    // no data found--just return empty QStringList
    if(fields.isEmpty())
    {
        return fields;
    }

    // Append empty final field(s) to get up to minFields
    while(fields.size() < minFields)
    {
        fields.append("");
    }

    return fields;
}


//////////////////
// Update current student info in table
//////////////////
void gruepr::refreshStudentDisplay()
{
    ui->dataDisplayTabWidget->setCurrentIndex(0);
    ui->studentTable->clear();
    ui->studentTable->setSortingEnabled(false); // have to disable sorting temporarily while adding items
    ui->studentTable->setColumnCount(dataOptions.sectionIncluded? 6 : 5);
    QIcon unsortedIcon(":/icons/updown_arrow.png");
    ui->studentTable->setHorizontalHeaderItem(0, new QTableWidgetItem(unsortedIcon, tr("Survey\nSubmission\nTime")));
    ui->studentTable->setHorizontalHeaderItem(1, new QTableWidgetItem(unsortedIcon, tr("First Name")));
    ui->studentTable->setHorizontalHeaderItem(2, new QTableWidgetItem(unsortedIcon, tr("Last Name")));
    int column = 3;
    if(dataOptions.sectionIncluded)
    {
        ui->studentTable->setHorizontalHeaderItem(column, new QTableWidgetItem(unsortedIcon, tr("Section")));
        column++;
    }
    ui->studentTable->setHorizontalHeaderItem(column, new QTableWidgetItem(tr("Edit")));
    column++;
    ui->studentTable->setHorizontalHeaderItem(column, new QTableWidgetItem(tr("Remove")));

    ui->studentTable->setRowCount(dataOptions.numStudentsInSystem);

    // compile all the student names and emails so that we can mark duplicates
    QStringList studentNames, studentEmails;
    for(int ID = 0; ID < dataOptions.numStudentsInSystem; ID++)
    {
        studentNames << (student[ID].firstname + student[ID].lastname);
        studentEmails << student[ID].email;
    }

    numStudents = 0;
    for(int ID = 0; ID < dataOptions.numStudentsInSystem; ID++)
    {
        bool duplicate = ((studentNames.count(student[ID].firstname + student[ID].lastname) > 1) || (studentEmails.count(student[ID].email) > 1));

        if((ui->sectionSelectionBox->currentIndex() == 0) || (student[ID].section == ui->sectionSelectionBox->currentText()))
        {
            QString studentToolTip = createAToolTip(student[ID], duplicate);

            TimestampTableWidgetItem *timestamp = new TimestampTableWidgetItem(student[ID].surveyTimestamp.toString(Qt::SystemLocaleShortDate));
            timestamp->setToolTip(studentToolTip);
            if(duplicate)
            {
                timestamp->setBackground(QBrush(QColor("#ffff3b")));
            }
            ui->studentTable->setItem(numStudents, 0, timestamp);

            QTableWidgetItem *firstName = new QTableWidgetItem(student[ID].firstname);
            firstName->setToolTip(studentToolTip);
            if(duplicate)
            {
                firstName->setBackground(QBrush(QColor("#ffff3b")));
            }
            ui->studentTable->setItem(numStudents, 1, firstName);

            QTableWidgetItem *lastName = new QTableWidgetItem(student[ID].lastname);
            lastName->setToolTip(studentToolTip);
            if(duplicate)
            {
                lastName->setBackground(QBrush(QColor("#ffff3b")));
            }
            ui->studentTable->setItem(numStudents, 2, lastName);

            int column = 3;
            if(dataOptions.sectionIncluded)
            {
                SectionTableWidgetItem *section = new SectionTableWidgetItem(student[ID].section);
                section->setToolTip(studentToolTip);
                if(duplicate)
                {
                    section->setBackground(QBrush(QColor("#ffff3b")));
                }
                ui->studentTable->setItem(numStudents, column, section);
                column++;
            }

            PushButtonThatSignalsMouseEnterEvents *editButton = new PushButtonThatSignalsMouseEnterEvents(QIcon(":/icons/edit.png"), "", this);
            editButton->setToolTip("<html>" + tr("Edit") + " " + student[ID].firstname + " " + student[ID].lastname + tr("'s data.") + "</html>");
            editButton->setProperty("StudentID", student[ID].ID);
            editButton->setProperty("duplicate", duplicate);
            if(duplicate)
            {
                editButton->setStyleSheet("QPushButton {background-color: #ffff3b; border: none;}");
            }
            connect(editButton, &PushButtonThatSignalsMouseEnterEvents::clicked, this, &gruepr::editAStudent);
            // pass on mouse enter events onto cell in table
            connect(editButton, &PushButtonThatSignalsMouseEnterEvents::mouseEntered, this, [this, editButton]
                        {int row=0; while(editButton != ui->studentTable->cellWidget(row, ui->studentTable->columnCount()-2)) {row++;} on_studentTable_cellEntered(row,0);});
            ui->studentTable->setCellWidget(numStudents, column, editButton);
            column++;

            PushButtonThatSignalsMouseEnterEvents *removerButton = new PushButtonThatSignalsMouseEnterEvents(QIcon(":/icons/delete.png"), "", this);
            removerButton->setToolTip("<html>" + tr("Remove") + " " + student[ID].firstname + " " + student[ID].lastname + " " + tr("from the current data set.") + "</html>");
            removerButton->setProperty("StudentID", student[ID].ID);
            removerButton->setProperty("duplicate", duplicate);
            if(duplicate)
            {
                removerButton->setStyleSheet("QPushButton {background-color: #ffff3b; border: none;}");
            }
            connect(removerButton, &PushButtonThatSignalsMouseEnterEvents::clicked, this, &gruepr::removeAStudent);
            // pass on mouse enter events onto cell in table
            connect(removerButton, &PushButtonThatSignalsMouseEnterEvents::mouseEntered, this, [this, removerButton]
                        {int row=0; while(removerButton != ui->studentTable->cellWidget(row, ui->studentTable->columnCount()-1)) {row++;} on_studentTable_cellEntered(row,0);});
            ui->studentTable->setCellWidget(numStudents, column, removerButton);

            numStudents++;
        }
    }
    ui->studentTable->setRowCount(numStudents);

    QString sectiontext = (ui->sectionSelectionBox->currentIndex() == 0? "All sections" : " Section: " + sectionName);
    ui->statusBar->showMessage(ui->statusBar->currentMessage().split("\u2192")[0].trimmed() + "  \u2192 " + sectiontext + "  \u2192 " + QString::number(numStudents) + " students");

    ui->studentTable->resizeColumnsToContents();
    ui->studentTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    ui->studentTable->setSortingEnabled(true);
}


////////////////////////////////////////////
// Create a tooltip for a student
////////////////////////////////////////////
QString gruepr::createAToolTip(const StudentRecord &info, bool duplicateRecord)
{
    QString toolTip = "<html>";
    if(duplicateRecord)
    {
        toolTip += "<table><tr><td bgcolor=#ffff3b><b>" + tr("There appears to be multiple survey submissions from this student!") + "</b></td></tr></table><br>";
    }
    toolTip += info.firstname + " " + info.lastname;
    toolTip += "<br>" + info.email;
    if(dataOptions.genderIncluded)
    {
        toolTip += "<br>" + tr("Gender") + ":  ";
        if(info.gender == StudentRecord::woman)
        {
            toolTip += tr("woman");
        }
        else if(info.gender == StudentRecord::man)
        {
            toolTip += tr("man");
        }
        else
        {
            toolTip += tr("nonbinary/unknown");
        }
    }
    if(dataOptions.URMIncluded)
    {
        toolTip += "<br>" + tr("Identity") + ":  ";
        toolTip += info.URMResponse;
    }
    for(int attribute = 0; attribute < dataOptions.numAttributes; attribute++)
    {
        toolTip += "<br>" + tr("Attribute ") + QString::number(attribute + 1) + ":  ";
        if(info.attribute[attribute] != -1)
        {
            if(dataOptions.attributeIsOrdered[attribute])
            {
                toolTip += QString::number(info.attribute[attribute]);
            }
            else
            {
                // if attribute has "unset/unknown" value of -1, char is nicely '?'; if attribute value is > 26, letters are repeated as needed
                toolTip += (info.attribute[attribute] <= 26 ? QString(char(info.attribute[attribute]-1 + 'A')) :
                                                              QString(char((info.attribute[attribute]-1)%26 + 'A')).repeated(1+((info.attribute[attribute]-1)/26)));
            }
        }
        else
        {
            toolTip += "?";
        }
    }
    if(!(info.availabilityChart.isEmpty()))
    {
        toolTip += "<br>--<br>" + info.availabilityChart;
    }
    if(dataOptions.notesIncluded)
    {
        QString note = info.notes;
        toolTip += "<br>--<br>" + tr("Notes") + ":<br>" + (note.isEmpty()? ("<i>" + tr("none") + "</i>") : note.replace("\n","<br>"));
    }
    toolTip += "</html>";

    return toolTip;
}


////////////////////////////////////////////
// Create and optimize teams using genetic algorithm
////////////////////////////////////////////
QList<int> gruepr::optimizeTeams(const int *studentIDs)
{
    // create and seed the pRNG (need to specifically do it here because this is happening in a new thread)
    std::random_device randDev;
    std::mt19937 pRNG(randDev());

    // Initialize an initial generation of random teammate sets, genePool[populationSize][numStudents].
    // Each genome in this generation stores (by permutation) which students are in which team.
    // Array has one entry per student and lists, in order, the "ID number" of the
    // student, referring to the order of the student in the students[] array.
    // For example, if team 1 has 4 students, and genePool[0][] = [4, 9, 12, 1, 3, 6...], then the first genome places
    // students[] entries 4, 9, 12, and 1 on to team 1 and students[] entries 3 and 6 as the first two students on team 2.

    // allocate memory for genepool and tempgenepool to hold the next generation as it is being created
    int **genePool = new int*[populationSize];
    int **tempPool = new int*[populationSize];
    // allocate memory for ancestors and tempancestors to hold the ancestors of the next generation as it is being created
    int **ancestors = new int*[populationSize];
    int **tempAncestors = new int*[populationSize];
    int numAncestors = 2;           //always track mom & dad
    for(int gen = 0; gen < numGenerationsOfAncestors; gen++)
    {
        numAncestors += (4<<gen);   //add an additional 2^(n+1) ancestors for the next level of (great)grandparents
    }
    for(int genome = 0; genome < populationSize; ++genome)
    {
        genePool[genome] = new int[numStudents];
        tempPool[genome] = new int[numStudents];
        ancestors[genome] = new int[numAncestors];
        tempAncestors[genome] = new int[numAncestors];
    }
    // allocate memory for array of indexes, to be sorted in order of score (so genePool[orderedIndex[0]] is the one with the top score)
    int *orderedIndex = new int[populationSize];
    for(int genome = 0; genome < populationSize; ++genome)
    {
        orderedIndex[genome] = genome;
    }

    // create an initial population
    // start with an array of all the student IDs in order
    int *randPerm = new int[numStudents];
    for(int i = 0; i < numStudents; i++)
    {
        randPerm[i] = studentIDs[i];
    }
    // then make "populationSize" number of random permutations for initial population, store in genePool
    for(int genome = 0; genome < populationSize; genome++)
    {
        std::shuffle(randPerm, randPerm+numStudents, pRNG);
        for(int ID = 0; ID < numStudents; ID++)
        {
            genePool[genome][ID] = randPerm[ID];
        }
    }
    delete[] randPerm;

    // just use random values for the initial "ancestor" values, since none of these are related and so all matings should be permitted
    std::uniform_int_distribution<unsigned int> randAncestor(0, populationSize);
    for(int genome = 0; genome < populationSize; genome++)
    {
        for(int ancestor = 0; ancestor < numAncestors; ancestor++)
        {
            ancestors[genome][ancestor] = randAncestor(pRNG);
        }
    }

    // calculate this first generation's scores (multi-threaded using OpenMP, preallocating one set of scoring variables per thread)
    QVector<float> scores(populationSize);
    float *unusedTeamScores, *schedScore;
    float **attributeScore;
    int *penaltyPoints;
#pragma omp parallel shared(scores) private(unusedTeamScores, attributeScore, schedScore, penaltyPoints)
{
    unusedTeamScores = new float[numTeams];
    attributeScore = new float*[dataOptions.numAttributes];
    for(int attrib = 0; attrib < dataOptions.numAttributes; attrib++)
    {
        attributeScore[attrib] = new float[numTeams];
    }
    schedScore = new float[numTeams];
    penaltyPoints = new int[numTeams];
#pragma omp for
    for(int genome = 0; genome < populationSize; genome++)
    {
        scores[genome] = getTeamScores(&genePool[genome][0], unusedTeamScores, attributeScore, schedScore, penaltyPoints);
    }
    delete[] penaltyPoints;
    delete[] schedScore;
    for(int attrib = 0; attrib < dataOptions.numAttributes; attrib++)
    {
        delete[] attributeScore[attrib];
    }
    delete[] attributeScore;
    delete[] unusedTeamScores;
}

    // get genome indexes in order of score, largest to smallest
    std::sort(orderedIndex, orderedIndex+populationSize, [&scores](const int i, const int j){return (scores.at(i) > scores.at(j));});
    emit generationComplete(scores, orderedIndex, 0, 0);

    int child[maxStudents];
    int *mom=nullptr, *dad=nullptr;                 // pointer to genome of mom and dad
    float bestScores[generationsOfStability]={0};	// historical record of best score in the genome, going back generationsOfStability generations
    int generation = 0;
    float scoreStability;
    bool localOptimizationStopped = false;
    int teamSize[maxTeams] = {0};
    for(int team = 0; team < numTeams; team++)
    {
        teamSize[team] = teams[team].size;
    }

    // now optimize
    do						// allow user to choose to continue optimizing beyond maxGenerations or seemingly reaching stability
    {
        do					// keep optimizing until reach stability or maxGenerations
        {
            // clone the elites in genePool into tempPool
            for(int genome = 0; genome < numElites; genome++)
            {
                for(int ID = 0; ID < numStudents; ID++)
                {
                    tempPool[genome][ID] = genePool[orderedIndex[genome]][ID];
                }
            }

            // create rest of population in tempPool by mating
            for(int genome = numElites; genome < populationSize; genome++)
            {
                //get a couple of parents
                GA::tournamentSelectParents(genePool, orderedIndex, ancestors, mom, dad, tempAncestors[genome], pRNG);

                //mate them and put child in tempPool
                GA::mate(mom, dad, teamSize, numTeams, child, numStudents, pRNG);
                for(int ID = 0; ID < numStudents; ID++)
                {
                    tempPool[genome][ID] = child[ID];
                }
            }

            // mutate all but the top scoring elite with some probability--if a mutation occurs, mutate same genome again with same probability
            std::uniform_int_distribution<unsigned int> randProbability(1, 100);
            for(int genome = 1; genome < populationSize; genome++)
            {
                while(randProbability(pRNG) < mutationLikelihood)
                {
                    GA::mutate(&tempPool[genome][0], numStudents, pRNG);
                }
            }

            // copy all of tempPool into genePool and all of tempAncestors into ancestors
            for(int genome = 0; genome < populationSize; genome++)
            {
                for(int ID = 0; ID < numStudents; ID++)
                {
                    genePool[genome][ID] = tempPool[genome][ID];
                }
                for(int ancestor = 0; ancestor < numAncestors; ancestor++)
                {
                    ancestors[genome][ancestor] = tempAncestors[genome][ancestor];
                }
            }

            generation++;

            // calculate new generation's scores (multi-threaded using OpenMP, preallocating one set of scoring variables per thread)
#pragma omp parallel shared(scores) private(unusedTeamScores, attributeScore, schedScore, penaltyPoints)
{
            unusedTeamScores = new float[numTeams];
            attributeScore = new float*[dataOptions.numAttributes];
            for(int attrib = 0; attrib < dataOptions.numAttributes; attrib++)
            {
                attributeScore[attrib] = new float[numTeams];
            }
            schedScore = new float[numTeams];
            penaltyPoints = new int[numTeams];
#pragma omp for
            for(int genome = 0; genome < populationSize; genome++)
            {
                scores[genome] = getTeamScores(&genePool[genome][0], unusedTeamScores, attributeScore, schedScore, penaltyPoints);
            }
            delete[] penaltyPoints;
            delete[] schedScore;
            for(int attrib = 0; attrib < dataOptions.numAttributes; attrib++)
            {
                delete[] attributeScore[attrib];
            }
            delete[] attributeScore;
            delete[] unusedTeamScores;
}

            // get genome indexes in order of score, largest to smallest
            std::sort(orderedIndex, orderedIndex+populationSize, [&scores](const int i, const int j){return (scores.at(i) > scores.at(j));});

            // determine best score, save in historical record, and calculate score stability
            bestScores[generation%generationsOfStability] = scores[orderedIndex[0]];	//the best scores from the most recent generationsOfStability, wrapping around the storage location
            auto mmScores = std::minmax_element(bestScores, bestScores+generationsOfStability);
            if(*mmScores.second == *mmScores.first)
            {
                scoreStability = scores[orderedIndex[0]] / (0.0001);
            }
            else
            {
                scoreStability = scores[orderedIndex[0]] / (*mmScores.second - *mmScores.first);
            }

            emit generationComplete(scores, orderedIndex, generation, scoreStability);

            optimizationStoppedmutex.lock();
            localOptimizationStopped = optimizationStopped;
            optimizationStoppedmutex.unlock();
        }
        while(!localOptimizationStopped && ((generation < minGenerations) || ((generation < maxGenerations) && (scoreStability < minScoreStability))));

        if(localOptimizationStopped)
        {
            keepOptimizing = false;
            emit turnOffBusyCursor();
        }
        else
        {
            keepOptimizing = true;
        }
    }
    while(keepOptimizing);

    finalGeneration = generation;
    teamSetScore = bestScores[generation%generationsOfStability];

    //copy best team set into a QList to return
    QList<int> bestTeamSet;
    bestTeamSet.reserve(numStudents);
    for(int ID = 0; ID < numStudents; ID++)
    {
        bestTeamSet << genePool[orderedIndex[0]][ID];
    }

    // deallocate memory
    for(int genome = 0; genome < populationSize; ++genome)
    {
        delete[] tempPool[genome];
        delete[] genePool[genome];
        delete[] tempAncestors[genome];
        delete[] ancestors[genome];
    }
    delete[] tempPool;
    delete[] genePool;
    delete[] tempAncestors;
    delete[] ancestors;
    delete[] orderedIndex;

    return bestTeamSet;
}


//////////////////
// Calculate team scores, returning the total score (which is, typically, the harmonic mean of all team scores)
//////////////////
float gruepr::getTeamScores(const int teammates[], float teamScores[], float **attributeScore, float *schedScore, int *penaltyPoints)
{
    // Initialize each component score
    for(int team = 0; team < numTeams; team++)
    {
        for(int attribute = 0; attribute < dataOptions.numAttributes; attribute++)
        {
            attributeScore[attribute][team] = 0;
        }
        schedScore[team] = 0;
        penaltyPoints[team] = 0;
    }

    int ID, firstStudentInTeam = 0;

    // Calculate each component score:

    // Calculate attribute scores and penalties for each attribute for each team:
    for(int attrib = 0; attrib < dataOptions.numAttributes; attrib++)
    {
        if((realAttributeWeights[attrib] > 0) || (haveAnyIncompatibleAttributes[attrib]))
        {
            ID = 0;
            for(int team = 0; team < numTeams; team++)
            {
                const int teamSize = teams[team].size;
                // gather all unique attribute values
                std::set<int> attributeLevelsInTeam;
                for(int teammate = 0; teammate < teamSize; teammate++)
                {
                    attributeLevelsInTeam.insert(student[teammates[ID]].attribute[attrib]);
                    ID++;
                }

                // Add a penalty per pair of incompatible attribute responses found
                if(haveAnyIncompatibleAttributes[attrib])
                {
                    // go through each pair found in teamingOptions.incompatibleAttributeValues[attrib] list and see if both int's found in attributeLevelsInTeam
                    for(auto pair : qAsConst(teamingOptions.incompatibleAttributeValues[attrib]))
                    {
                        if((attributeLevelsInTeam.count(pair.first) != 0) && (attributeLevelsInTeam.count(pair.second) != 0))
                        {
                            penaltyPoints[team]++;
                        }
                    }
                }

                if(realAttributeWeights[attrib] > 0)
                {
                    // Remove attribute values of -1 (unknown/not set) and then determine attribute scores
                    attributeLevelsInTeam.erase(-1);
                    int attributeRangeInTeam;
                    if(dataOptions.attributeIsOrdered[attrib])
                    {
                        // attribute has meaningful ordering/numerical values--heterogeneous means create maximum spread between max and min values
                        attributeRangeInTeam = *attributeLevelsInTeam.rbegin() - *attributeLevelsInTeam.begin();    // std::set is stored in order; rbegin() is last element
                    }
                    else
                    {
                        // attribute is categorical--heterogeneous means create maximum number of unique values
                        attributeRangeInTeam = attributeLevelsInTeam.size() - 1;
                    }

                    attributeScore[attrib][team] = float(attributeRangeInTeam) / (dataOptions.attributeMax[attrib] - dataOptions.attributeMin[attrib]);
                    if(teamingOptions.desireHomogeneous[attrib])	//attribute scores are 0 if homogeneous and +1 if full range of values are in a team, so flip if want homogeneous
                    {
                        attributeScore[attrib][team] = 1 - attributeScore[attrib][team];
                    }
                }

                attributeScore[attrib][team] *= realAttributeWeights[attrib];
            }
        }
    }

    // Calculate schedule scores for each team:
    if(realScheduleWeight > 0)
    {
        const int numDays = dataOptions.dayNames.size();
        const int numTimes = dataOptions.timeNames.size();
        const int numTimeSlots = numDays * numTimes;

        ID = 0;
        for(int team = 0; team < numTeams; team++)
        {
            const int teamSize = teams[team].size;
            int firstStudentInTeam = ID;

            // combine each student's schedule array into a team schedule array
            bool teamAvailability[numTimeSlots];
            for(int time = 0; time < numTimeSlots; time++)
            {
                ID = firstStudentInTeam;
                teamAvailability[time] = true;
                for(int teammate = 0; teammate < teamSize; teammate++)
                {
                    if(!student[teammates[ID]].ambiguousSchedule)
                    {
                        teamAvailability[time] = teamAvailability[time] && !student[teammates[ID]].unavailable[time];	// logical "and" each student's not-unavailability
                    }
                    ID++;
                }
            }

            // count number of students with ambiguous schedules
            int numStudentsWithAmbiguousSchedules = 0;
            ID = firstStudentInTeam;
            for(int teammate = 0; teammate < teamSize; teammate++)
            {
                if(student[teammates[ID]].ambiguousSchedule)
                {
                    numStudentsWithAmbiguousSchedules++;
                }
                ID++;
            }

            // keep schedule score at 0 unless 2+ students have unambiguous sched (avoid runaway score by grouping students w/ambiguous scheds)
            if((teamSize <= 2) || (numStudentsWithAmbiguousSchedules < (teamSize-2)))
            {
                // count how many free time blocks there are
                if(teamingOptions.meetingBlockSize == 1)
                {
                    for(int time = 0; time < numTimeSlots; time++)
                    {
                        if(teamAvailability[time])
                        {
                            schedScore[team]++;
                        }
                    }
                }
                else    //user wants to count only 2-hr time blocks, but don't count wrap-around block from end of 1 day to beginning of next!
                {
                    for(int day = 0; day < numDays; day++)
                    {
                        for(int time = 0; time < numTimes-1; time++)
                        {
                            if(teamAvailability[(day*numTimes)+time])
                            {
                                time++;
                                if(teamAvailability[(day*numTimes)+time])
                                {
                                    schedScore[team]++;
                                }
                            }
                        }
                    }
                }

                // convert counts to a schedule score
                if(schedScore[team] > teamingOptions.desiredTimeBlocksOverlap)		// if team has more than desiredTimeBlocksOverlap, the "extra credit" is 1/6 of the additional overlaps
                {
                    schedScore[team] = 1 + ((schedScore[team] - teamingOptions.desiredTimeBlocksOverlap) / (6*teamingOptions.desiredTimeBlocksOverlap));
                }
                else if(schedScore[team] >= teamingOptions.minTimeBlocksOverlap)	// if team has between minimum and desired amount of schedule overlap
                {
                    schedScore[team] /= teamingOptions.desiredTimeBlocksOverlap;	// normal schedule score is number of overlaps / desired number of overlaps
                }
                else													// if team has fewer than minTimeBlocksOverlap, apply penalty
                {
                    schedScore[team] = 0;
                    penaltyPoints[team]++;
                }

                schedScore[team] *= realScheduleWeight;
            }
        }
    }

    // Determine gender adjustments
    if(dataOptions.genderIncluded && (teamingOptions.isolatedWomenPrevented || teamingOptions.isolatedMenPrevented || teamingOptions.singleGenderPrevented))
    {
        ID = 0;
        for(int team = 0; team < numTeams; team++)
        {
            const int teamSize = teams[team].size;

            // Count how many of each gender on the team
            int numWomen = 0;
            int numMen = 0;
            for(int teammate = 0; teammate < teamSize; teammate++)
            {
                if(student[teammates[ID]].gender == StudentRecord::man)
                {
                    numMen++;
                }
                else if(student[teammates[ID]].gender == StudentRecord::woman)
                {
                    numWomen++;
                }
                ID++;
            }

            // Apply penalties as appropriate
            if(teamingOptions.isolatedWomenPrevented && numWomen == 1)
            {
                penaltyPoints[team]++;
            }
            if(teamingOptions.isolatedMenPrevented && numMen == 1)
            {
                penaltyPoints[team]++;
            }
            if(teamingOptions.singleGenderPrevented && (numMen == 0 || numWomen == 0))
            {
                penaltyPoints[team]++;
            }
        }
    }

    // Determine URM adjustments
    if(dataOptions.URMIncluded && teamingOptions.isolatedURMPrevented)
    {
        ID = 0;
        for(int team = 0; team < numTeams; team++)
        {
            const int teamSize = teams[team].size;
            int numURM = 0;

            // Count how many URM on the team
            for(int teammate = 0; teammate < teamSize; teammate++)
            {
                if(student[teammates[ID]].URM)
                {
                    numURM++;
                }
                ID++;
            }

            // Apply penalties as appropriate
            if(numURM == 1)
            {
                penaltyPoints[team]++;
            }
        }
    }

    // Determine adjustments for required teammates NOT on same team
    if(haveAnyRequiredTeammates)
    {
        bool noSectionChosen = (ui->sectionSelectionBox->currentIndex() == 0);
        firstStudentInTeam=0;
        // Loop through each team
        for(int team = 0; team < numTeams; team++)
        {
            const int teamSize = teams[team].size;
            //loop through all students in team
            for(int studentA = firstStudentInTeam; studentA < (firstStudentInTeam + teamSize); studentA++)
            {
                const bool *studentAsRequireds = student[teammates[studentA]].requiredWith;
                //loop through ALL other students
                for(int studentB = 0; studentB < numStudents; studentB++)
                {
                    const int studB = teammates[studentB];
                    //if this pairing is required and studentB is in a/the section being teamed, then we need to see if they are, in fact, teammates
                    if(studentAsRequireds[studB] && (noSectionChosen || student[studB].section == sectionName))
                    {
                        //loop through all of studentA's current teammates until if/when we find studentB
                        int currMates = firstStudentInTeam;
                        while((teammates[currMates] != studB) && currMates < (firstStudentInTeam + teamSize))
                        {
                            currMates++;
                        }
                        //if the pairing was not found, then adjustment = -realNumScoringFactors
                        if(teammates[currMates] != studB)
                        {
                            penaltyPoints[team]++;
                        }
                    }
                }
            }
            firstStudentInTeam += teamSize;
        }
    }

    // Determine adjustments for prevented teammates on same team
    if(haveAnyPreventedTeammates)
    {
        firstStudentInTeam=0;
        // Loop through each team
        for(int team = 0; team < numTeams; team++)
        {
            const int teamSize = teams[team].size;
            //loop studentA from first student in team to 2nd-to-last student in team
            for(int studentA = firstStudentInTeam; studentA < (firstStudentInTeam + (teamSize-1)); studentA++)
            {
                const bool *studentAsPreventeds = student[teammates[studentA]].preventedWith;
                //loop studentB from studentA+1 to last student in team
                for(int studentB = (studentA+1); studentB < (firstStudentInTeam + teamSize); studentB++)
                {
                    //if pairing prevented, adjustment = -realNumScoringFactors
                    if(studentAsPreventeds[teammates[studentB]])
                    {
                        penaltyPoints[team]++;
                    }
                }
            }
            firstStudentInTeam += teamSize;
        }
    }

    // Determine adjustments for not having at least N requested teammates
    if(haveAnyRequestedTeammates)
    {
        firstStudentInTeam = 0;
        int numRequestedTeammates = 0, numRequestedTeammatesFound = 0;
        // Loop through each team
        for(int team = 0; team < numTeams; team++)
        {
            const int teamSize = teams[team].size;
            //loop studentA from first student in team to last student in team
            for(int studentA = firstStudentInTeam; studentA < (firstStudentInTeam + teamSize); studentA++)
            {
                const bool *studentAsRequesteds = student[teammates[studentA]].requestedWith;
                numRequestedTeammates = 0;
                //first count how many teammates this student has requested
                for(int ID = 0; ID < numStudents; ID++)
                {
                    if(studentAsRequesteds[ID])
                    {
                        numRequestedTeammates++;
                    }
                }
                if(numRequestedTeammates > 0)
                {
                    numRequestedTeammatesFound = 0;
                    //next loop count how many requested teammates are found on their team
                    for(int studentB = firstStudentInTeam; studentB < (firstStudentInTeam + teamSize); studentB++)
                    {
                        if(studentAsRequesteds[teammates[studentB]])
                        {
                            numRequestedTeammatesFound++;
                        }
                    }
                    //apply penalty if student has unfulfilled requests that exceed the number allowed
                    if(numRequestedTeammatesFound < std::min(numRequestedTeammates, teamingOptions.numberRequestedTeammatesGiven))
                    {
                        penaltyPoints[team]++;
                    }
                }
            }
            firstStudentInTeam += teamSize;
        }
    }

    //Bring component scores together for final team scores and, ultimately, a net score:
    //final team scores are normalized to be out of 100 (but with possible "extra credit" for more than desiredTimeBlocksOverlap hours w/ 100% team availability)
    for(int team = 0; team < numTeams; team++)
    {

        // remove the schedule extra credit if any penalties are being applied, so that a very high schedule overlap doesn't cancel out the penalty
        if((schedScore[team] > realScheduleWeight) && (penaltyPoints[team] > 0))
        {
            schedScore[team] = realScheduleWeight;
        }

        teamScores[team] = schedScore[team];
        for(int attribute = 0; attribute < dataOptions.numAttributes; attribute++)
        {
            teamScores[team] += attributeScore[attribute][team];
        }
        teamScores[team] = 100 * ((teamScores[team] / float(realNumScoringFactors)) - penaltyPoints[team]);
    }

    //Use the harmonic mean for the "total score"
    //This value, the inverse of the average of the inverses, is skewed towards the smaller members so that we optimize for better values of the worse teams
    float harmonicSum = 0;
    for(int team = 0; team < numTeams; team++)
    {
        //very poor teams have 0 or negative scores, and this makes the harmonic mean meaningless
        //if any teamScore is <= 0, return the arithmetic mean punished by reducing towards negative infinity by half the arithmetic mean
        if(teamScores[team] <= 0)
        {
            float mean = std::accumulate(teamScores, teamScores+numTeams, float(0.0))/float(numTeams);		// accumulate() is from <numeric>, and it sums an array
            if(mean < 0)
            {
                return(mean + (mean/2));
            }
            else
            {
                return(mean - (mean/2));
            }
        }
        harmonicSum += 1/teamScores[team];
    }

    return(float(numTeams)/harmonicSum);
}


//////////////////
// Update current team info
//////////////////
void gruepr::refreshTeamInfo(QList<int> teamNums)
{
    // if no teamNums given, update all
    if(teamNums == QList<int>({-1}))
    {
        teamNums.clear();
        for(int team = 0; team < numTeams; team++)
        {
            teamNums << team;
        }
    }

    // get scores for each team
    int *genome = new int[numStudents];
    int ID = 0;
    for(int team = 0; team < numTeams; team++)
    {
        for(int teammate = 0; teammate < teams[team].size; teammate++)
        {
            genome[ID] = teams[team].studentIDs[teammate];
            ID++;
        }
    }
    float *teamScores = new float[numTeams];
    float **attributeScore = new float*[dataOptions.numAttributes];
    for(int attrib = 0; attrib < dataOptions.numAttributes; attrib++)
    {
        attributeScore[attrib] = new float[numTeams];
    }
    float *schedScore = new float[numTeams];
    int *penaltyPoints = new int[numTeams];
    getTeamScores(genome, teamScores, attributeScore, schedScore, penaltyPoints);
    for(int team = 0; team < numTeams; team++)
    {
        teams[team].score = teamScores[team];
    }
    delete[] penaltyPoints;
    delete[] schedScore;
    for(int attrib = 0; attrib < dataOptions.numAttributes; attrib++)
    {
        delete[] attributeScore[attrib];
    }
    delete[] attributeScore;
    delete[] teamScores;
    delete[] genome;

    //determine other team info
    for(QList<int>::iterator team = teamNums.begin(); team != teamNums.end(); team++)
    {
        //re-zero values
        teams[*team].numWomen = 0;
        teams[*team].numMen = 0;
        teams[*team].numNeither = 0;
        teams[*team].numURM = 0;
        teams[*team].numStudentsWithAmbiguousSchedules = 0;
        for(int attribute = 0; attribute < dataOptions.numAttributes; attribute++)
        {
            teams[*team].attributeVals[attribute].clear();
        }
        for(int day = 0; day < dataOptions.dayNames.size(); day++)
        {
            for(int time = 0; time < dataOptions.timeNames.size(); time++)
            {
                teams[*team].numStudentsAvailable[day][time] = 0;
            }
        }

        //set values
        for(int teammate = 0; teammate < teams[*team].size; teammate++)
        {
            if(dataOptions.genderIncluded)
            {
                if(student[teams[*team].studentIDs[teammate]].gender == StudentRecord::woman)
                {
                    teams[*team].numWomen++;
                }
                else if(student[teams[*team].studentIDs[teammate]].gender == StudentRecord::man)
                {
                    teams[*team].numMen++;
                }
                else
                {
                    teams[*team].numNeither++;
                }
            }
            if(dataOptions.URMIncluded)
            {
                if(student[teams[*team].studentIDs[teammate]].URM)
                {
                    teams[*team].numURM++;
                }
            }
            for(int attribute = 0; attribute < dataOptions.numAttributes; attribute++)
            {
                if(!teams[*team].attributeVals[attribute].contains(student[teams[*team].studentIDs[teammate]].attribute[attribute]))
                {
                    teams[*team].attributeVals[attribute] << student[teams[*team].studentIDs[teammate]].attribute[attribute];
                }
            }
            if(!student[teams[*team].studentIDs[teammate]].ambiguousSchedule)
            {
                for(int day = 0; day < dataOptions.dayNames.size(); day++)
                {
                    for(int time = 0; time < dataOptions.timeNames.size(); time++)
                    {
                        if(!student[teams[*team].studentIDs[teammate]].unavailable[(day*dataOptions.timeNames.size())+time])
                        {
                            teams[*team].numStudentsAvailable[day][time]++;
                        }
                    }
                }
            }
            else
            {
                teams[*team].numStudentsWithAmbiguousSchedules++;
            }
        }
    }
}


//////////////////
// Update current team tooltips
//////////////////
void gruepr::refreshTeamToolTips(QList<int> teamNums)
{
    // if no teamNums given, update all
    if(teamNums == QList<int>({-1}))
    {
        teamNums.clear();
        for(int team = 0; team < numTeams; team++)
        {
            teamNums << team;
        }
    }


    // create tooltips
    for(QList<int>::iterator team = teamNums.begin(); team != teamNums.end(); team++)
    {
        teams[*team].tooltip = "<html>" + tr("Team ") + teams[*team].name + "<br>";
        if(dataOptions.genderIncluded)
        {
            teams[*team].tooltip += tr("Gender") + ":  ";
            if(teams[*team].numWomen > 0)
            {
                teams[*team].tooltip += QString::number(teams[*team].numWomen) + (teams[*team].numWomen > 1? tr(" women") : tr(" woman"));
            }
            if(teams[*team].numWomen > 0 && teams[*team].numMen > 0)
            {
                teams[*team].tooltip += ", ";
            }
            if(teams[*team].numMen > 0)
            {
                teams[*team].tooltip += QString::number(teams[*team].numMen) + (teams[*team].numMen > 1? tr(" men") : tr(" man"));
            }
            if((teams[*team].numWomen > 0 || teams[*team].numMen > 0) && teams[*team].numNeither > 0)
            {
                teams[*team].tooltip += ", ";
            }
            if(teams[*team].numNeither > 0)
            {
                teams[*team].tooltip += QString::number(teams[*team].numNeither) + tr(" non-binary/unknown");
            }
        }
        if(dataOptions.URMIncluded)
        {
            teams[*team].tooltip += "<br>" + tr("URM") + ":  " + QString::number(teams[*team].numURM);
        }
        for(int attribute = 0; attribute < dataOptions.numAttributes; attribute++)
        {
            teams[*team].tooltip += "<br>" + tr("Attribute ") + QString::number(attribute + 1) + ":  ";
            if(dataOptions.attributeIsOrdered[attribute])
            {
                // attribute is ordered/numbered, so important info is the range of values (but ignore any "unset/unknown" values of -1)
                QList<int> teamVals = teams[*team].attributeVals[attribute];
                teamVals.removeAll(-1);
                auto mm = std::minmax_element(teamVals.begin(), teamVals.end());
                if(*mm.first == *mm.second)
                {
                    teams[*team].tooltip += QString::number(*mm.first);
                }
                else
                {
                    teams[*team].tooltip += QString::number(*mm.first) + " - " + QString::number(*mm.second);
                }
            }
            else
            {
                // attribute is categorical, so important info is the list of values
                std::sort(teams[*team].attributeVals[attribute].begin(), teams[*team].attributeVals[attribute].end());
                // if attribute has "unset/unknown" value of -1, char is nicely '?'; if attribute value is > 26, letters are repeated as needed
                teams[*team].tooltip += (teams[*team].attributeVals[attribute].at(0) <= 26 ? QString(char(teams[*team].attributeVals[attribute].at(0)-1 + 'A')) :
                                         QString(char((teams[*team].attributeVals[attribute].at(0)-1)%26 + 'A')).repeated(1+((teams[*team].attributeVals[attribute].at(0)-1)/26)));
                for(int val = 1; val < teams[*team].attributeVals[attribute].size(); val++)
                {
                    teams[*team].tooltip += ", " + (teams[*team].attributeVals[attribute].at(val) <= 26 ? QString(char(teams[*team].attributeVals[attribute].at(val)-1 + 'A')) :
                        QString(char((teams[*team].attributeVals[attribute].at(val)-1)%26 + 'A')).repeated(1+((teams[*team].attributeVals[attribute].at(val)-1)/26)));
                }
            }
        }
        if(!dataOptions.dayNames.isEmpty())
        {
            teams[*team].tooltip += "<br>--<br>" + tr("Availability:") + "<table style='padding: 0px 3px 0px 3px;'><tr><th></th>";

            for(int day = 0; day < dataOptions.dayNames.size(); day++)
            {
                // using first 3 characters in day name as abbreviation
                teams[*team].tooltip += "<th>" + dataOptions.dayNames.at(day).left(3) + "</th>";
            }
            teams[*team].tooltip += "</tr>";

            for(int time = 0; time < dataOptions.timeNames.size(); time++)
            {
                teams[*team].tooltip += "<tr><th>" + dataOptions.timeNames.at(time) + "</th>";
                for(int day = 0; day < dataOptions.dayNames.size(); day++)
                {
                    QString percentage = QString::number((100*teams[*team].numStudentsAvailable[day][time]) / (teams[*team].size-teams[*team].numStudentsWithAmbiguousSchedules)) + "% ";
                    if(percentage == "100% ")
                    {
                        teams[*team].tooltip += "<td align='center' bgcolor='PaleGreen'><b>" + percentage + "</b></td>";
                    }
                    else
                    {
                        teams[*team].tooltip += "<td align='center'>" + percentage + "</td>";
                    }
                }
                teams[*team].tooltip += "</tr>";
            }
            teams[*team].tooltip += "</table></html>";
        }
    }
}


//////////////////
// Update current team info in tree display as well as the text output options
//////////////////
void gruepr::resetTeamDisplay()
{
    ui->dataDisplayTabWidget->setCurrentIndex(1);
    teamDataTree->setColumnCount(3 + (dataOptions.genderIncluded? 1 : 0) + (dataOptions.URMIncluded? 1 : 0) +
                                      dataOptions.numAttributes + ((!dataOptions.dayNames.isEmpty())? 1 : 0) );   // name, gender?, URM?, each attribute, schedule?
    QStringList headerLabels;
    headerLabels << tr("name") << tr("team\nscore");
    if(dataOptions.genderIncluded)
    {
        headerLabels << tr("gender");
    }
    if(dataOptions.URMIncluded)
    {
        headerLabels << tr("URM");
    }
    for(int attribute = 0; attribute < dataOptions.numAttributes; attribute++)
    {
        headerLabels << tr("attribute ") + QString::number(attribute+1);
    }
    if(!dataOptions.dayNames.isEmpty())
    {
        headerLabels << tr("available\nmeeting\nhours");
    }
    headerLabels << tr("display_order");
    for(int i = 0; i < headerLabels.size()-1; i++)
    {
        teamDataTree->showColumn(i);
    }
    teamDataTree->hideColumn(headerLabels.size()-1);
    auto *headerTextWithIcon = new QTreeWidgetItem;
    for(int i = 0; i < headerLabels.size(); i++)
    {
        headerTextWithIcon->setIcon(i, QIcon(":/icons/updown_arrow.png"));
        headerTextWithIcon->setText(i, headerLabels.at(i));
    }
    teamDataTree->setHeaderItem(headerTextWithIcon);
    teamDataTree->setSortingEnabled(false);
    teamDataTree->header()->setDefaultAlignment(Qt::AlignCenter);
    teamDataTree->header()->setSectionResizeMode(QHeaderView::Interactive);
    teamDataTree->setFocus();


    // before clearing table, remember which teams are shown expanded and which are collapsed
    delete[] expanded;
    expanded = new bool[numTeams];
    for(int team = 0; team < numTeams; team++)
    {
        expanded[team] = false;
    }

    // remove and delete all parent items
    qDeleteAll(parentItem);
    parentItem.clear();
    teamDataTree->clear();

    // create a new parent item for each team
    for(int team = 0; team < numTeams; team++)
    {
        parentItem.append(new TeamTreeWidgetItem(teamDataTree));
    }
}


//////////////////
// Update current team info in tree display as well as the text output options
//////////////////
void gruepr::refreshTeamDisplay(QList<int> teamNums)
{
    QApplication::setOverrideCursor(QCursor(Qt::BusyCursor));

    // if no teamNums given, update all
    if(teamNums == QList<int>({-1}))
    {
        teamNums.clear();
        for(int team = 0; team < numTeams; team++)
        {
            teamNums << team;
        }
    }

    //iterate through teams to update the tree of teams and students
    int numTeamsUpdated = 0;
    for(QList<int>::iterator team = teamNums.begin(); team != teamNums.end(); team++)
    {
        //create team items and fill in information
        int column = 0;
        parentItem[*team]->setText(column, tr("Team ") + teams[*team].name);
        parentItem[*team]->setTextAlignment(column, Qt::AlignLeft | Qt::AlignVCenter);
        parentItem[*team]->setData(column, TeamInfoDisplay, tr("Team ") + teams[*team].name);
        parentItem[*team]->setData(column, TeamInfoSort, student[teams[*team].studentIDs[0]].lastname+student[teams[*team].studentIDs[0]].firstname);
        parentItem[*team]->setData(column, TeamNumber, *team);
        parentItem[*team]->setToolTip(column, teams[*team].tooltip);
        column++;
        parentItem[*team]->setText(column, QString::number(double(teams[*team].score), 'f', 2));
        parentItem[*team]->setTextAlignment(column, Qt::AlignLeft | Qt::AlignVCenter);
        parentItem[*team]->setData(column, TeamInfoDisplay, QString::number(double(teams[*team].score), 'f', 2));
        parentItem[*team]->setData(column, TeamInfoSort, teams[*team].score);
        parentItem[*team]->setToolTip(column, teams[*team].tooltip);
        column++;
        if(dataOptions.genderIncluded)
        {
            QString genderText;
            if(teams[*team].numWomen > 0)
            {
                genderText += QString::number(teams[*team].numWomen) + tr("W");
            }
            if(teams[*team].numWomen > 0 && (teams[*team].numMen > 0 || teams[*team].numNeither > 0))
            {
                genderText += ", ";
            }
            if(teams[*team].numMen > 0)
            {
                genderText += QString::number(teams[*team].numMen) + tr("M");
            }
            if(teams[*team].numMen > 0 && teams[*team].numNeither > 0)
            {
                genderText += ", ";
            }
            if(teams[*team].numNeither > 0)
            {
                genderText += QString::number(teams[*team].numNeither) + tr("X");
            }
            parentItem[*team]->setData(column, TeamInfoDisplay, genderText);
            parentItem[*team]->setData(column, TeamInfoSort, teams[*team].numMen - teams[*team].numWomen);
            parentItem[*team]->setToolTip(column, teams[*team].tooltip);
            parentItem[*team]->setTextAlignment(column, Qt::AlignCenter);
            column++;
        }
        if(dataOptions.URMIncluded)
        {
            parentItem[*team]->setData(column, TeamInfoDisplay, QString::number(teams[*team].numURM));
            parentItem[*team]->setData(column, TeamInfoSort, teams[*team].numURM);
            parentItem[*team]->setToolTip(column, teams[*team].tooltip);
            parentItem[*team]->setTextAlignment(column, Qt::AlignCenter);
            column++;
        }
        for(int attribute = 0; attribute < dataOptions.numAttributes; attribute++)
        {
            QString attributeText;
            int sortData;
            QList<int> teamVals = teams[*team].attributeVals[attribute];
            teamVals.removeAll(-1);
            if(dataOptions.attributeIsOrdered[attribute])
            {
                // attribute is ordered/numbered, so important info is the range of values
                auto mm = std::minmax_element(teamVals.begin(), teamVals.end());
                if(*mm.first == *mm.second)
                {
                    attributeText = QString::number(*mm.first);
                }
                else
                {
                    attributeText = QString::number(*mm.first) + " - " + QString::number(*mm.second);
                }
                sortData = *mm.first * 1000 + *mm.second;
            }
            else
            {
                // attribute is categorical, so important info is the list of values
                std::sort(teamVals.begin(), teamVals.end());
                // if attribute has "unset/unknown" value of -1, char is nicely '?'; if attribute value is > 26, letters are repeated as needed
                attributeText = (teamVals.at(0) <= 26 ? QString(char(teamVals.at(0)-1 + 'A')) : QString(char((teamVals.at(0)-1)%26 + 'A')).repeated(1+((teamVals.at(0)-1)/26)));
                for(int val = 1; val < teamVals.size(); val++)
                {
                    attributeText += ", ";
                    attributeText += (teamVals.at(val) <= 26 ? QString(char(teamVals.at(val)-1 + 'A')) : QString(char((teamVals.at(val)-1)%26 + 'A')).repeated(1+((teamVals.at(val)-1)/26)));
                }
                sortData = (teamVals.at(0) * 10000) + (teamVals.size() * 100) + (teamVals.size() > 1 ? teamVals.at(1) : 0);   // sort by first item, then number of items, then second item
            }
            parentItem[*team]->setData(column, TeamInfoDisplay, attributeText);
            parentItem[*team]->setData(column, TeamInfoSort, sortData);
            parentItem[*team]->setToolTip(column, teams[*team].tooltip);
            parentItem[*team]->setTextAlignment(column, Qt::AlignCenter);
            column++;
        }
        if(!dataOptions.dayNames.isEmpty())
        {
            parentItem[*team]->setData(column, TeamInfoDisplay, QString::number(teams[*team].tooltip.count("100%")));
            parentItem[*team]->setData(column, TeamInfoSort, teams[*team].tooltip.count("100%"));
            parentItem[*team]->setToolTip(column, teams[*team].tooltip);
            parentItem[*team]->setTextAlignment(column, Qt::AlignCenter);
            column++;
        }

        // expand each team now, but remember if we should re-collapse later
        expanded[*team] = parentItem[*team]->isExpanded();
        parentItem[*team]->setExpanded(true);

        //remove all student items in the team
        foreach(auto i, parentItem[*team]->takeChildren())
        {
            parentItem[*team]->removeChild(i);
            delete i;
        }

        //add new student items
        for(int stud = 0; stud < teams[*team].size; stud++)
        {
            QString studentToolTip = createAToolTip(student[teams[*team].studentIDs[stud]], false);
            auto *childItem = new TeamTreeWidgetItem(parentItem[*team]);
            int column = 0;
            childItem->setText(column, student[teams[*team].studentIDs[stud]].firstname + " " + student[teams[*team].studentIDs[stud]].lastname);
            childItem->setData(column, Qt::UserRole, student[teams[*team].studentIDs[stud]].ID);
            childItem->setToolTip(column, studentToolTip);
            childItem->setTextAlignment(column, Qt::AlignLeft | Qt::AlignVCenter);
            teamDataTree->resizeColumnToContents(column);
            column++;
            column++;   // skip the teamscore column
            if(dataOptions.genderIncluded)
            {
                if(student[teams[*team].studentIDs[stud]].gender == StudentRecord::woman)
                {
                   childItem->setText(column,tr("woman"));

                }
                else if(student[teams[*team].studentIDs[stud]].gender == StudentRecord::man)
                {
                    childItem->setText(column,tr("man"));
                }
                else
                {
                    childItem->setText(column,tr("non-binary/unknown"));
                }
                childItem->setToolTip(column, studentToolTip);
                childItem->setTextAlignment(column, Qt::AlignCenter);
                teamDataTree->resizeColumnToContents(column);
                column++;
            }
            if(dataOptions.URMIncluded)
            {
                if(student[teams[*team].studentIDs[stud]].URM)
                {
                   childItem->setText(column,tr("yes"));

                }
                else
                {
                    childItem->setText(column,"");
                }
                childItem->setToolTip(column, studentToolTip);
                childItem->setTextAlignment(column, Qt::AlignCenter);
                teamDataTree->resizeColumnToContents(column);
                column++;
            }
            for(int attribute = 0; attribute < dataOptions.numAttributes; attribute++)
            {
                int value = student[teams[*team].studentIDs[stud]].attribute[attribute];
                if(value != -1)
                {
                    if(dataOptions.attributeIsOrdered[attribute])
                    {
                        childItem->setText(column, QString::number(value));
                    }
                    else
                    {
                        childItem->setText(column, (value <= 26 ? QString(char(value-1 + 'A')) : QString(char((value-1)%26 + 'A')).repeated(1+((value-1)/26))));
                    }
                }
                else
                {
                   childItem->setText(column, "?");
                }
                childItem->setToolTip(column, studentToolTip);
                childItem->setTextAlignment(column, Qt::AlignCenter);
                teamDataTree->resizeColumnToContents(column);
                column++;
            }
            if(!dataOptions.dayNames.isEmpty())
            {
                int availableTimes = student[teams[*team].studentIDs[stud]].availabilityChart.count("√");
                childItem->setText(column, availableTimes == 0? "--" : QString::number(availableTimes));
                childItem->setToolTip(column, studentToolTip);
                childItem->setTextAlignment(column, Qt::AlignCenter);
            }
        }

        parentItem[*team]->setExpanded(expanded[*team]);

        // to prevent hanging while tree is populated, redisplay every 10 teams
        if(++numTeamsUpdated%10 == 0)
        {
            QCoreApplication::processEvents();
        }
    }

    teamDataTree->setSortingEnabled(true);
    setWindowModified(true);
    QApplication::restoreOverrideCursor();
}


//////////////////
//Setup printer and then print paginated file(s) in boxes
//////////////////
void gruepr::createFileContents()
{
    spreadsheetFileContents = tr("Section") + "\t" + tr("Team") + "\t" + tr("Name") + "\t" + tr("Email") + "\n";

    instructorsFileContents = tr("File: ") + dataOptions.dataFile.filePath() + "\n" + tr("Section: ") + sectionName + "\n" + tr("Optimized over ") +
            QString::number(finalGeneration) + tr(" generations") + "\n" + tr("Net score: ") + QString::number(double(teamSetScore)) + "\n\n";
    instructorsFileContents += tr("Teaming Options") + ":";
    if(dataOptions.genderIncluded)
    {
        instructorsFileContents += (teamingOptions.isolatedWomenPrevented? ("\n" + tr("Isolated women prevented")) : "");
        instructorsFileContents += (teamingOptions.isolatedMenPrevented? ("\n" + tr("Isolated men prevented")) : "");
        instructorsFileContents += (teamingOptions.singleGenderPrevented? ("\n" + tr("Single gender teams prevented")) : "");
    }
    if(dataOptions.URMIncluded && teamingOptions.isolatedURMPrevented)
    {
        instructorsFileContents += "\n" + tr("Isolated URM students prevented");
    }
    if(teamingOptions.scheduleWeight > 0)
    {
        instructorsFileContents += "\n" + tr("Meeting block size is ") + QString::number(teamingOptions.meetingBlockSize) + tr(" hours");
        instructorsFileContents += "\n" + tr("Minimum number of meeting times = ") + QString::number(teamingOptions.minTimeBlocksOverlap);
        instructorsFileContents += "\n" + tr("Desired number of meeting times = ") + QString::number(teamingOptions.desiredTimeBlocksOverlap);
        instructorsFileContents += "\n" + tr("Schedule weight = ") + QString::number(double(teamingOptions.scheduleWeight));
    }
    for(int attrib = 0; attrib < dataOptions.numAttributes; attrib++)
    {
        instructorsFileContents += "\n" + tr("Attribute ") + QString::number(attrib+1) + ": "
                + tr("weight") + " = " + QString::number(double(teamingOptions.attributeWeights[attrib])) +
                + ", " + (teamingOptions.desireHomogeneous[attrib]? tr("homogeneous") : tr("heterogeneous"));
    }
    instructorsFileContents += "\n\n\n";
    for(int attrib = 0; attrib < dataOptions.numAttributes; attrib++)
    {
        QString questionWithResponses = tr("Attribute ") + QString::number(attrib+1) + "\n" + dataOptions.attributeQuestionText.at(attrib) + "\n" + tr("Responses:");
        for(int response = 0; response < dataOptions.attributeQuestionResponses[attrib].size(); response++)
        {
            if(dataOptions.attributeIsOrdered[attrib])
            {
                questionWithResponses += "\n\t" + dataOptions.attributeQuestionResponses[attrib].at(response);
            }
            else
            {
                // show response with a preceding letter (letter repeated for responses after 26)
                questionWithResponses += "\n\t" + (response < 26 ? QString(char(response + 'A')) : QString(char(response%26 + 'A')).repeated(1 + (response/26)));
                questionWithResponses += ". " + dataOptions.attributeQuestionResponses[attrib].at(response);
            }
        }
        questionWithResponses += "\n\n\n";
        instructorsFileContents += questionWithResponses;
    }

    studentsFileContents = "";

    // get team numbers in the order that they are currently displayed/sorted
    QList<int> teamDisplayNum;
    for(int row = 0; row < numTeams; row++)
    {
        int team = 0;
        while(teamDataTree->topLevelItem(row)->data(teamDataTree->columnCount()-1, TeamInfoSort).toInt() != team)
        {
            team++;
        }
        teamDisplayNum << teamDataTree->topLevelItem(row)->data(0, TeamNumber).toInt();
    }

    //loop through every team
    for(int teamNum = 0; teamNum < numTeams; teamNum++)
    {
        int team = teamDisplayNum.at(teamNum);
        instructorsFileContents += tr("Team ") + teams[team].name + tr("  -  Score = ") + QString::number(double(teams[team].score), 'f', 2) + "\n\n";
        studentsFileContents += tr("Team ") + teams[team].name + "\n\n";

        //loop through each teammate in the team
        for(int teammate = 0; teammate < teams[team].size; teammate++)
        {
            if(dataOptions.genderIncluded)
            {
                if(student[teams[team].studentIDs[teammate]].gender == StudentRecord::woman)
                {
                    instructorsFileContents += " woman ";
                }
                else if(student[teams[team].studentIDs[teammate]].gender == StudentRecord::man)
                {
                    instructorsFileContents += "  man  ";
                }
                else
                {
                    instructorsFileContents += "   x   ";
                }
            }
            if(dataOptions.URMIncluded)
            {
                if(student[teams[team].studentIDs[teammate]].URM)
                {
                    instructorsFileContents += " URM ";
                }
                else
                {
                    instructorsFileContents += "     ";
                }
            }
            for(int attribute = 0; attribute < dataOptions.numAttributes; attribute++)
            {
                int value = student[teams[team].studentIDs[teammate]].attribute[attribute];
                if(value != -1)
                {
                    if(dataOptions.attributeIsOrdered[attribute])
                    {
                        instructorsFileContents += (QString::number(value)).leftJustified(3);
                    }
                    else
                    {
                        instructorsFileContents += (value <= 26 ? (QString(char(value-1 + 'A'))).leftJustified(3) : (QString(char((value-1)%26 + 'A')).repeated(1+((value-1)/26)))).leftJustified(3);
                    }
                }
                else
                {
                    instructorsFileContents += (QString("?")).leftJustified(3);
                }
            }
            int nameSize = (student[teams[team].studentIDs[teammate]].firstname + " " + student[teams[team].studentIDs[teammate]].lastname).size();
            instructorsFileContents += student[teams[team].studentIDs[teammate]].firstname + " " + student[teams[team].studentIDs[teammate]].lastname +
                    QString(std::max(2,30-nameSize), ' ') + student[teams[team].studentIDs[teammate]].email + "\n";
            studentsFileContents += student[teams[team].studentIDs[teammate]].firstname + " " + student[teams[team].studentIDs[teammate]].lastname +
                    QString(std::max(2,30-nameSize), ' ') + student[teams[team].studentIDs[teammate]].email + "\n";
            spreadsheetFileContents += student[teams[team].studentIDs[teammate]].section + "\t" + teams[team].name + "\t" + student[teams[team].studentIDs[teammate]].firstname +
                    " " + student[teams[team].studentIDs[teammate]].lastname + "\t" + student[teams[team].studentIDs[teammate]].email + "\n";

        }
        if(!dataOptions.dayNames.isEmpty())
        {
            instructorsFileContents += "\n" + tr("Availability:") + "\n            ";
            studentsFileContents += "\n" + tr("Availability:") + "\n            ";

            for(int day = 0; day < dataOptions.dayNames.size(); day++)
            {
                // using first 3 characters in day name as abbreviation
                instructorsFileContents += "  " + dataOptions.dayNames.at(day).left(3) + "  ";
                studentsFileContents += "  " + dataOptions.dayNames.at(day).left(3) + "  ";
            }
            instructorsFileContents += "\n";
            studentsFileContents += "\n";

            for(int time = 0; time < dataOptions.timeNames.size(); time++)
            {
                instructorsFileContents += dataOptions.timeNames.at(time) + QString((11-dataOptions.timeNames.at(time).size()), ' ');
                studentsFileContents += dataOptions.timeNames.at(time) + QString((11-dataOptions.timeNames.at(time).size()), ' ');
                for(int day = 0; day < dataOptions.dayNames.size(); day++)
                {
                    QString percentage = QString::number((100*teams[team].numStudentsAvailable[day][time]) / (teams[team].size-teams[team].numStudentsWithAmbiguousSchedules)) + "% ";
                    instructorsFileContents += QString((4+dataOptions.dayNames.at(day).leftRef(3).size())-percentage.size(), ' ') + percentage;
                    studentsFileContents += QString((4+dataOptions.dayNames.at(day).leftRef(3).size())-percentage.size(), ' ') + percentage;
                }
                instructorsFileContents += "\n";
                studentsFileContents += "\n";
            }
        }
        instructorsFileContents += "\n\n";
        studentsFileContents += "\n\n";
    }
}


//////////////////
//Setup printer and then print paginated file(s) in boxes
//////////////////
void gruepr::printFiles(bool printInstructorsFile, bool printStudentsFile, bool printSpreadsheetFile, bool printToPDF)
{
    // connecting to the printer is spun off into a separate thread because sometimes it causes ~30 second hang
    // message box explains what's happening
    QApplication::setOverrideCursor(QCursor(Qt::BusyCursor));
    auto *msgBox = new QMessageBox(this);
    msgBox->setIcon(QMessageBox::Information);
    msgBox->setText(printToPDF? tr("Setting up PDF writer...") : tr("Connecting to printer..."));
    msgBox->setStandardButtons(nullptr);        // no buttons
    msgBox->setModal(false);
    msgBox->show();
    QEventLoop loop;
    connect(this, &gruepr::connectedToPrinter, &loop, &QEventLoop::quit);
    QPrinter *printer = nullptr;
    QFuture<QPrinter*> future = QtConcurrent::run(this, &gruepr::setupPrinter);
    loop.exec();
    printer = future.result();
    msgBox->close();
    msgBox->deleteLater();
    QApplication::restoreOverrideCursor();

    bool doIt;
    QString baseFileName;
    if(printToPDF)
    {
        printer->setOutputFormat(QPrinter::PdfFormat);
        baseFileName = QFileDialog::getSaveFileName(this, tr("Choose a location and base filename"), "", tr("PDF File (*.pdf);;All Files (*)"));
        doIt = !(baseFileName.isEmpty());
    }
    else
    {
        printer->setOutputFormat(QPrinter::NativeFormat);
        QPrintDialog printDialog(printer);
        printDialog.setWindowTitle(tr("Print"));
        doIt = (printDialog.exec() == QDialog::Accepted);
    }

    if(doIt)
    {
        QFont printFont = QFont("Oxygen Mono", 10, QFont::Normal);

        if(printInstructorsFile)
        {
            if(printToPDF)
            {
                QString fileName = QFileInfo(baseFileName).path() + "/" + QFileInfo(baseFileName).completeBaseName() + "_instructor." + QFileInfo(baseFileName).suffix();
                printer->setOutputFileName(fileName);
            }
            printOneFile(instructorsFileContents, "\n\n\n", printFont, printer);
        }
        if(printStudentsFile)
        {
            if(printToPDF)
            {
                QString fileName = QFileInfo(baseFileName).path() + "/" + QFileInfo(baseFileName).completeBaseName() + "_student." + QFileInfo(baseFileName).suffix();
                printer->setOutputFileName(fileName);
            }
            printOneFile(studentsFileContents, "\n\n\n", printFont, printer);

        }
        if(printSpreadsheetFile)
        {
            if(printToPDF)
            {
                QString fileName = QFileInfo(baseFileName).path() + "/" + QFileInfo(baseFileName).completeBaseName() + "_spreadsheet." + QFileInfo(baseFileName).suffix();
                printer->setOutputFileName(fileName);
            }
            QTextDocument textDocument(spreadsheetFileContents, this);
            printFont.setPointSize(9);
            textDocument.setDefaultFont(printFont);
            printer->setOrientation(QPrinter::Landscape);
            textDocument.print(printer);
        }
        setWindowModified(false);
    }
    delete printer;
}

QPrinter* gruepr::setupPrinter()
{
    auto *printer = new QPrinter(QPrinter::HighResolution);
    printer->setOrientation(QPrinter::Portrait);
    emit connectedToPrinter();
    return printer;
}

void gruepr::printOneFile(const QString &file, const QString &delimiter, QFont &font, QPrinter *printer)
{
    QPainter painter(printer);
    painter.setFont(font);
    QFont titleFont = font;
    titleFont.setBold(true);
    int LargeGap = printer->logicalDpiY()/2, MediumGap = LargeGap/2, SmallGap = MediumGap/2;
    int pageHeight = painter.window().height() - 2*LargeGap;

    QStringList eachTeam = file.split(delimiter, QString::SkipEmptyParts);

    //paginate the output
    QStringList currentPage;
    QList<QStringList> pages;
    int y = 0;
    QStringList::const_iterator it = eachTeam.cbegin();
    while (it != eachTeam.cend())
    {
        //calculate height on page of this team text
        int textWidth = painter.window().width() - 2*LargeGap - 2*SmallGap;
        int maxHeight = painter.window().height();
        QRect textRect = painter.boundingRect(0, 0, textWidth, maxHeight, Qt::TextWordWrap, *it);
        int height = textRect.height() + 2*SmallGap;
        if(y + height > pageHeight && !currentPage.isEmpty())
        {
            pages.push_back(currentPage);
            currentPage.clear();
            y = 0;
        }
        currentPage.push_back(*it);
        y += height + MediumGap;
        ++it;
    }
    if (!currentPage.isEmpty())
    {
        pages.push_back(currentPage);
    }

    //print each page, 1 at a time
    for (int pagenum = 0; pagenum < pages.size(); pagenum++)
    {
        if (pagenum > 0)
        {
            printer->newPage();
        }
        QTransform savedTransform = painter.worldTransform();
        painter.translate(0, LargeGap);
        QStringList::const_iterator it = pages[pagenum].cbegin();
        while (it != pages[pagenum].cend())
        {
            QString title = it->left(it->indexOf('\n')) + " ";
            QString body = it->right(it->size() - (it->indexOf('\n')+1));
            int boxWidth = painter.window().width() - 2*LargeGap;
            int textWidth = boxWidth - 2*SmallGap;
            int maxHeight = painter.window().height();
            QRect titleRect = painter.boundingRect(LargeGap+SmallGap, SmallGap, textWidth, maxHeight, Qt::TextWordWrap, title);
            QRect bodyRect = painter.boundingRect(LargeGap+SmallGap, SmallGap+titleRect.height(), textWidth, maxHeight, Qt::TextWordWrap, body);
            int boxHeight = titleRect.height() + bodyRect.height() + 2 * SmallGap;
            painter.setPen(QPen(Qt::black, 2, Qt::SolidLine));
            painter.setBrush(Qt::white);
            painter.drawRect(LargeGap, 0, boxWidth, boxHeight);
            painter.setFont(titleFont);
            painter.drawText(titleRect, Qt::TextWordWrap, title);
            painter.setFont(font);
            painter.drawText(bodyRect, Qt::TextWordWrap, body);
            painter.translate(0, boxHeight);
            painter.translate(0, MediumGap);
            ++it;
        }
        painter.setWorldTransform(savedTransform);
        painter.drawText(painter.window(), Qt::AlignHCenter | Qt::AlignBottom, QString::number(pagenum + 1));
    }
}


//////////////////
// Before closing the main application window, see if we want to save the current settings as defaults
//////////////////
void gruepr::closeEvent(QCloseEvent *event)
{
    QSettings savedSettings;
    savedSettings.setValue("windowGeometry", saveGeometry());
    savedSettings.setValue("dataFileLocation", dataOptions.dataFile.canonicalFilePath());
    bool dontActuallyExit = false;

    if(savedSettings.value("askToSaveDefaultsOnExit",true).toBool())
    {
        QApplication::beep();
        QMessageBox saveOptionsOnClose(this);
        saveOptionsOnClose.setWindowFlags(Qt::Dialog | Qt::MSWindowsFixedSizeDialogHint | Qt::WindowTitleHint);
        QCheckBox neverShowAgain(tr("Don't ask me this again."), &saveOptionsOnClose);

        saveOptionsOnClose.setIcon(QMessageBox::Question);
        saveOptionsOnClose.setWindowTitle(tr("Save Options?"));
        saveOptionsOnClose.setText(tr("Before exiting, should we save all of the\ncurrent teaming options as defaults?"));
        saveOptionsOnClose.setCheckBox(&neverShowAgain);
        saveOptionsOnClose.setStandardButtons(QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
        saveOptionsOnClose.setButtonText(QMessageBox::Discard, tr("Don't Save"));

        saveOptionsOnClose.exec();
        if(saveOptionsOnClose.result() == QMessageBox::Save)
        {
            savedSettings.setValue("idealTeamSize", ui->idealTeamSizeBox->value());
            savedSettings.setValue("isolatedWomenPrevented", teamingOptions.isolatedWomenPrevented);
            savedSettings.setValue("isolatedMenPrevented", teamingOptions.isolatedMenPrevented);
            savedSettings.setValue("singleGenderPrevented", teamingOptions.singleGenderPrevented);
            savedSettings.setValue("isolatedURMPrevented", teamingOptions.isolatedURMPrevented);
            savedSettings.setValue("minTimeBlocksOverlap", teamingOptions.minTimeBlocksOverlap);
            savedSettings.setValue("desiredTimeBlocksOverlap", teamingOptions.desiredTimeBlocksOverlap);
            savedSettings.setValue("meetingBlockSize", teamingOptions.meetingBlockSize);
            savedSettings.setValue("scheduleWeight", ui->scheduleWeight->value());
            savedSettings.beginWriteArray("Attributes");
            for (int attribNum = 0; attribNum < maxAttributes; ++attribNum)
            {
                savedSettings.setArrayIndex(attribNum);
                savedSettings.setValue("desireHomogeneous", teamingOptions.desireHomogeneous[attribNum]);
                savedSettings.setValue("weight", teamingOptions.attributeWeights[attribNum]);
                savedSettings.remove("incompatibleResponses");  //clear any existing values
                savedSettings.beginWriteArray("incompatibleResponses");
                for(int incompResp = 0; incompResp < teamingOptions.incompatibleAttributeValues[attribNum].size(); incompResp++)
                {
                    savedSettings.setArrayIndex(incompResp);
                    savedSettings.setValue("incompatibleResponses",
                            (QString::number(teamingOptions.incompatibleAttributeValues[attribNum].at(incompResp).first) + "," +
                             QString::number(teamingOptions.incompatibleAttributeValues[attribNum].at(incompResp).second)));
                }
                savedSettings.endArray();
            }
            savedSettings.endArray();
            savedSettings.setValue("requestedTeammateNumber", ui->requestedTeammateNumberBox->value());
        }
        else if(saveOptionsOnClose.result() == QMessageBox::Cancel)
        {
            dontActuallyExit = true;
        }

        if(neverShowAgain.checkState() == Qt::Checked)
        {
            savedSettings.setValue("askToSaveDefaultsOnExit", false);
        }
    }

    if(dontActuallyExit)
    {
        event->ignore();
    }
    else
    {
        event->accept();
    }
}
